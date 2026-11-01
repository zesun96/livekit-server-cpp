use crate::{models, Error, NativeLibrary, Result};
use serde_json::{json, Value};
use std::{fmt, sync::Arc};

/// Binary protobuf request and ordered HTTP header snapshot. Honor timeout and TLS validation.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct HttpRequest {
    pub url: String,
    pub method: String,
    pub headers: Vec<(String, String)>,
    pub body: Vec<u8>,
    pub timeout_ms: i64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct HttpResponse {
    pub status_code: i32,
    pub body: Vec<u8>,
}

pub type Transport = dyn Fn(&HttpRequest) -> crate::CallbackResult<HttpResponse> + Send + Sync;

#[derive(Clone)]
pub struct ApiOptions {
    pub url: String,
    pub api_key: String,
    pub api_secret: String,
    pub access_token: String,
    pub timeout_ms: i64,
    pub transport: Option<Arc<Transport>>,
}

impl Default for ApiOptions {
    fn default() -> Self {
        Self {
            url: String::new(),
            api_key: String::new(),
            api_secret: String::new(),
            access_token: String::new(),
            timeout_ms: 30_000,
            transport: None,
        }
    }
}

impl fmt::Debug for ApiOptions {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ApiOptions")
            .field("credentials", &"[redacted]")
            .finish_non_exhaustive()
    }
}

impl ApiOptions {
    fn fields(&self) -> Value {
        json!({"url": self.url, "api_key": self.api_key, "api_secret": self.api_secret,
            "access_token": self.access_token, "timeout_ms": self.timeout_ms.to_string(),
            "use_transport": self.transport.is_some()})
    }
}

/// All six services; clones retain the module and application transport through Arc ownership.
#[derive(Clone, Debug)]
pub struct LiveKitApi {
    native: NativeLibrary,
    options: ApiOptions,
}

impl LiveKitApi {
    pub fn new(native: NativeLibrary, options: ApiOptions) -> Result<Self> {
        if options.timeout_ms <= 0 {
            return Err(Error::InvalidArgument(
                "Timeout must be positive milliseconds".into(),
            ));
        }
        Ok(Self { native, options })
    }

    pub(crate) fn call(&self, operation: &str, request: &str) -> Result<String> {
        let value = self.native.invoke(
            json!({"operation": operation, "request": request,
            "options": self.options.fields()}),
            self.options.transport.as_deref(),
            None,
        )?;
        value
            .as_str()
            .map(str::to_owned)
            .ok_or_else(|| Error::Binding("Expected native JSON string".into()))
    }

    pub(crate) fn get_dispatch(
        &self,
        dispatch_id: &str,
        room: &str,
    ) -> Result<Option<models::AgentDispatch>> {
        let value = self.native.invoke(
            json!({"operation": "get_dispatch", "dispatch_id": dispatch_id,
            "room": room, "options": self.options.fields()}),
            self.options.transport.as_deref(),
            None,
        )?;
        if value.is_null() {
            return Ok(None);
        }
        let json = value
            .as_str()
            .ok_or_else(|| Error::Binding("Expected dispatch JSON string or null".into()))?;
        Ok(Some(models::AgentDispatch::from_json(json)))
    }
}
