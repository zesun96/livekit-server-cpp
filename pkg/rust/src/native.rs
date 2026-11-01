use crate::{Error, HttpRequest, KeyProvider, Result, Transport};
use base64::{engine::general_purpose::URL_SAFE_NO_PAD, Engine};
use libloading::Library;
use serde::Deserialize;
use serde_json::{json, Value};
use std::{
    any::Any,
    ffi::{c_char, c_void, OsStr, OsString},
    fmt,
    panic::{self, AssertUnwindSafe},
    slice,
    sync::Arc,
};

type Callback =
    unsafe extern "C" fn(*mut c_void, *const c_char, usize, *mut *const c_char, *mut usize) -> i32;
type Invoke = unsafe extern "C" fn(
    *const c_char,
    usize,
    Option<Callback>,
    *mut c_void,
    *mut usize,
) -> *mut c_void;
type Free = unsafe extern "C" fn(*mut c_void);

struct Inner {
    invoke: Invoke,
    free: Free,
    // Keep the allocating library alive through all invocations and result cleanup.
    _library: Library,
}

/// Cloneable shared module ownership. Each invocation has independent callback state.
#[derive(Clone)]
pub struct NativeLibrary(Arc<Inner>);

impl fmt::Debug for NativeLibrary {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("NativeLibrary")
            .field("abi_version", &1)
            .finish_non_exhaustive()
    }
}

impl NativeLibrary {
    /// Loads a shared SDK and checks ABI version 1 before any SDK operation.
    ///
    /// # Safety
    /// The path and its loader dependencies must identify a trusted LiveKit server SDK implementing
    /// the complete `c_api.h` ABI 1 contract, including synchronous, same-thread callbacks, valid
    /// buffers, no escaping C++ exceptions, and support for concurrent independent invocations.
    /// Initializers/destructors must be safe in this process. The version check cannot prove this.
    pub unsafe fn load(path: impl AsRef<OsStr>) -> Result<Self> {
        // SAFETY: the caller supplies the documented library and dependency trust guarantee.
        let library = unsafe { Library::new(path) }.map_err(|e| Error::Load(e.to_string()))?;
        // SAFETY: ABI types and exports are part of the caller's contract; copy function addresses
        // only while retaining their owning library in Inner.
        let (version, invoke, free) = unsafe {
            (
                *library
                    .get::<unsafe extern "C" fn() -> i32>(b"lks_abi_version\0")
                    .map_err(|e| Error::Load(e.to_string()))?,
                *library
                    .get::<Invoke>(b"lks_invoke\0")
                    .map_err(|e| Error::Load(e.to_string()))?,
                *library
                    .get::<Free>(b"lks_free\0")
                    .map_err(|e| Error::Load(e.to_string()))?,
            )
        };
        // SAFETY: validated signature supplied by the caller's ABI contract.
        if unsafe { version() } != 1 {
            return Err(Error::Load(
                "Unsupported LiveKit server C ABI version (expected 1)".into(),
            ));
        }
        Ok(Self(Arc::new(Inner {
            invoke,
            free,
            _library: library,
        })))
    }

    /// Uses LIVEKIT_SERVER_RUST_LIBRARY or the platform's shared-library filename.
    ///
    /// # Safety
    /// The selected path, environment, OS search paths and dependencies must satisfy [`Self::load`].
    pub unsafe fn from_env() -> Result<Self> {
        let path = std::env::var_os("LIVEKIT_SERVER_RUST_LIBRARY")
            .filter(|p| !p.is_empty())
            .unwrap_or_else(|| {
                OsString::from(if cfg!(windows) {
                    "livekitserver.dll"
                } else if cfg!(target_os = "macos") {
                    "liblivekitserver.dylib"
                } else {
                    "liblivekitserver.so"
                })
            });
        // SAFETY: the caller extends load's contract to environment and loader discovery.
        unsafe { Self::load(path) }
    }

    pub(crate) fn invoke(
        &self,
        input: Value,
        transport: Option<&Transport>,
        key: Option<&KeyProvider>,
    ) -> Result<Value> {
        let input = serde_json::to_vec(&input)?;
        let mut context = Context {
            transport,
            key,
            buffers: Vec::new(),
            error: None,
            panic: None,
        };
        let mut length = 0;
        let callback = if transport.is_some() || key.is_some() {
            Some(exchange as Callback)
        } else {
            None
        };
        // SAFETY: input/context are live until this synchronous call returns. The ABI uses size_t
        // lengths, cdecl C callbacks and opaque context. Independent calls use independent contexts.
        let pointer = unsafe {
            (self.0.invoke)(
                input.as_ptr().cast(),
                input.len(),
                callback,
                (&mut context as *mut Context<'_>).cast(),
                &mut length,
            )
        };
        let output = Output {
            pointer,
            native: &self.0,
        };
        // Prioritize the original host failure, but always free the native result first.
        if let Some(payload) = context.panic.take() {
            drop(output);
            panic::resume_unwind(payload);
        }
        if let Some(error) = context.error.take() {
            return Err(error);
        }
        if pointer.is_null() {
            return Err(Error::Binding(
                "Native C ABI could not allocate or encode its result".into(),
            ));
        }
        // SAFETY: ABI contract guarantees a readable output_length-byte allocation. Output's Drop
        // calls the allocating library's free exactly once even if JSON parsing fails.
        let bytes = unsafe { slice::from_raw_parts(pointer.cast::<u8>(), length) };
        decode_envelope(bytes)
    }
}

struct Output<'a> {
    pointer: *mut c_void,
    native: &'a Inner,
}
impl Drop for Output<'_> {
    fn drop(&mut self) {
        if !self.pointer.is_null() {
            // SAFETY: pointer is an unfreed result from this same live native library.
            unsafe { (self.native.free)(self.pointer) };
        }
    }
}

fn decode_envelope(bytes: &[u8]) -> Result<Value> {
    #[derive(Deserialize)]
    struct Failure {
        code: i32,
        message: String,
        http_status: i32,
        twirp_code: String,
    }
    let value: Value = serde_json::from_slice(bytes)?;
    match value.get("ok").and_then(Value::as_bool) {
        Some(true) => value
            .get("value")
            .cloned()
            .ok_or_else(|| Error::Binding("Missing native value".into())),
        Some(false) => {
            let error: Failure = serde_json::from_value(value)?;
            Err(Error::Native {
                code: error.code,
                message: error.message,
                http_status: error.http_status,
                twirp_code: error.twirp_code,
            })
        }
        None => Err(Error::Binding("Invalid native result envelope".into())),
    }
}

struct Context<'a> {
    transport: Option<&'a Transport>,
    key: Option<&'a KeyProvider>,
    buffers: Vec<Vec<u8>>,
    error: Option<Error>,
    panic: Option<Box<dyn Any + Send>>,
}

impl Context<'_> {
    fn exchange(&mut self, request: &[u8]) -> Result<Vec<u8>> {
        #[derive(Deserialize)]
        #[serde(tag = "kind")]
        enum Request {
            #[serde(rename = "transport")]
            Transport {
                url: String,
                method: String,
                headers: Vec<(String, String)>,
                body: String,
                timeout_ms: String,
            },
            #[serde(rename = "key")]
            Key { key: String },
        }
        let response = match serde_json::from_slice(request)? {
            Request::Transport {
                url,
                method,
                headers,
                body,
                timeout_ms,
            } => {
                let transport = self
                    .transport
                    .ok_or_else(|| Error::Binding("Transport callback is required".into()))?;
                let request = HttpRequest {
                    url,
                    method,
                    headers,
                    body: URL_SAFE_NO_PAD
                        .decode(body)
                        .map_err(|e| Error::Binding(e.to_string()))?,
                    timeout_ms: timeout_ms
                        .parse()
                        .map_err(|_| Error::Binding("Invalid callback timeout".into()))?,
                };
                let response = transport(&request).map_err(Error::Callback)?;
                json!({"status_code": response.status_code.to_string(), "body": URL_SAFE_NO_PAD.encode(response.body)})
            }
            Request::Key { key } => {
                let provider = self
                    .key
                    .ok_or_else(|| Error::Binding("Key provider is required".into()))?;
                json!({"secret": provider(&key).map_err(Error::Callback)?.unwrap_or_default()})
            }
        };
        Ok(serde_json::to_vec(&response)?)
    }
}

unsafe extern "C" fn exchange(
    user: *mut c_void,
    request: *const c_char,
    length: usize,
    response: *mut *const c_char,
    response_length: *mut usize,
) -> i32 {
    // SAFETY: only invoked by the trusted synchronous SDK with the live per-call context and
    // readable/writable ABI buffers. No Rust panic may cross this function's C boundary.
    let context = unsafe { &mut *user.cast::<Context<'_>>() };
    if context.error.is_some() || context.panic.is_some() {
        return 1;
    }
    let result = panic::catch_unwind(AssertUnwindSafe(|| {
        // SAFETY: request is borrowed from native for this callback's duration.
        let request = unsafe { slice::from_raw_parts(request.cast::<u8>(), length) };
        let bytes = context.exchange(request)?;
        context.buffers.push(bytes);
        let bytes = context.buffers.last().expect("buffer just pushed");
        // SAFETY: native supplies writable response slots. Inner Vec allocations stay fixed
        // until invoke returns, even when the outer Vec reallocates during further callbacks.
        unsafe {
            *response = bytes.as_ptr().cast();
            *response_length = bytes.len();
        }
        Ok::<_, Error>(())
    }));
    match result {
        Ok(Ok(())) => 0,
        Ok(Err(error)) => {
            context.error = Some(error);
            1
        }
        Err(payload) => {
            context.panic = Some(payload);
            1
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{
        ffi::CString,
        sync::atomic::{AtomicUsize, Ordering},
    };

    static FREES: AtomicUsize = AtomicUsize::new(0);

    unsafe extern "C" fn mock_free(pointer: *mut c_void) {
        // SAFETY: the mock only returns CString::into_raw allocations, each freed once here.
        drop(unsafe { CString::from_raw(pointer.cast()) });
        FREES.fetch_add(1, Ordering::SeqCst);
    }

    unsafe extern "C" fn mock_invoke(
        input: *const c_char,
        length: usize,
        callback: Option<Callback>,
        user: *mut c_void,
        output_length: *mut usize,
    ) -> *mut c_void {
        // SAFETY: invoked only by NativeLibrary::invoke with live input and output slots.
        let input: Value =
            serde_json::from_slice(unsafe { slice::from_raw_parts(input.cast::<u8>(), length) })
                .unwrap();
        let mode = input["mode"].as_str().unwrap();
        let bytes = match mode {
            "null" => Vec::new(),
            "invalid_json" => vec![255],
            "native_error" => {
                br#"{"ok":false,"code":123,"message":"future","http_status":0,"twirp_code":""}"#
                    .to_vec()
            }
            "callback" | "callback_null" => {
                let request = br#"{"kind":"transport","url":"https://offline.invalid","method":"POST","headers":[],"body":"","timeout_ms":"1"}"#;
                let mut first = std::ptr::null();
                let mut first_length = 0;
                // SAFETY: mock follows the synchronous ABI; context and response slots are valid.
                let status = unsafe {
                    callback.unwrap()(
                        user,
                        request.as_ptr().cast(),
                        request.len(),
                        &mut first,
                        &mut first_length,
                    )
                };
                if status == 0 {
                    let mut second = std::ptr::null();
                    let mut second_length = 0;
                    // SAFETY: same synchronous per-call context; first buffer must remain alive.
                    assert_eq!(
                        unsafe {
                            callback.unwrap()(
                                user,
                                request.as_ptr().cast(),
                                request.len(),
                                &mut second,
                                &mut second_length,
                            )
                        },
                        0
                    );
                    // SAFETY: callback response allocations are borrowed until this mock returns.
                    let first: Value = serde_json::from_slice(unsafe {
                        slice::from_raw_parts(first.cast::<u8>(), first_length)
                    })
                    .unwrap();
                    let second: Value = serde_json::from_slice(unsafe {
                        slice::from_raw_parts(second.cast::<u8>(), second_length)
                    })
                    .unwrap();
                    assert_eq!(first["body"], "AAE");
                    assert_eq!(second["body"], "AAE");
                }
                if mode == "callback_null" {
                    Vec::new()
                } else {
                    br#"{"ok":true,"value":"callback"}"#.to_vec()
                }
            }
            _ => br#"{"ok":true,"value":"success"}"#.to_vec(),
        };
        // SAFETY: caller provides a writable size_t output slot.
        unsafe {
            *output_length = bytes.len();
        }
        if bytes.is_empty() {
            return std::ptr::null_mut();
        }
        CString::new(bytes).unwrap().into_raw().cast()
    }

    #[test]
    fn result_ownership_on_success_errors_null_and_panic() {
        #[cfg(windows)]
        let library: Library = libloading::os::windows::Library::this().unwrap().into();
        #[cfg(unix)]
        let library: Library = libloading::os::unix::Library::this().into();
        // Test fixture function addresses live in the current process module, whose handle is kept.
        let native = NativeLibrary(Arc::new(Inner {
            invoke: mock_invoke,
            free: mock_free,
            _library: library,
        }));
        FREES.store(0, Ordering::SeqCst);
        assert_eq!(
            native
                .invoke(json!({"mode":"success"}), None, None)
                .unwrap(),
            "success"
        );
        assert_eq!(FREES.load(Ordering::SeqCst), 1);
        assert!(matches!(
            native.invoke(json!({"mode":"native_error"}), None, None),
            Err(Error::Native { code: 123, .. })
        ));
        assert_eq!(FREES.load(Ordering::SeqCst), 2);
        assert!(native
            .invoke(json!({"mode":"invalid_json"}), None, None)
            .is_err());
        assert_eq!(FREES.load(Ordering::SeqCst), 3);
        assert!(native.invoke(json!({"mode":"null"}), None, None).is_err());
        assert_eq!(FREES.load(Ordering::SeqCst), 3);
        let transport = |_: &HttpRequest| {
            Ok(crate::HttpResponse {
                status_code: 200,
                body: vec![0, 1],
            })
        };
        native
            .invoke(json!({"mode":"callback"}), Some(&transport), None)
            .unwrap();
        assert_eq!(FREES.load(Ordering::SeqCst), 4);
        let failed = |_: &HttpRequest| Err(std::io::Error::other("host sentinel").into());
        assert!(matches!(
            native.invoke(json!({"mode":"callback"}), Some(&failed), None),
            Err(Error::Callback(_))
        ));
        assert_eq!(FREES.load(Ordering::SeqCst), 5);
        assert!(matches!(
            native.invoke(json!({"mode":"callback_null"}), Some(&failed), None),
            Err(Error::Callback(_))
        ));
        assert_eq!(FREES.load(Ordering::SeqCst), 5);
        let panicked = |_: &HttpRequest| panic::panic_any(123u32);
        let payload = panic::catch_unwind(AssertUnwindSafe(|| {
            native.invoke(json!({"mode":"callback"}), Some(&panicked), None)
        }))
        .unwrap_err();
        assert_eq!(payload.downcast_ref::<u32>(), Some(&123));
        assert_eq!(FREES.load(Ordering::SeqCst), 6);
    }

    #[test]
    fn unknown_errors_and_invalid_envelopes() {
        assert!(matches!(decode_envelope(br#"{"ok":false,"code":123,"message":"future","http_status":403,"twirp_code":"denied"}"#),
            Err(Error::Native { code: 123, http_status: 403, .. })));
        for input in [
            br#"{}"#.as_slice(),
            br#"{"ok":true}"#,
            br#"{"ok":1}"#,
            b"not json",
        ] {
            assert!(decode_envelope(input).is_err());
        }
        assert_eq!(
            decode_envelope(br#"{"ok":true,"value":null}"#).unwrap(),
            Value::Null
        );
    }
}
