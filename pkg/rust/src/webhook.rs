use crate::{CallbackResult, Error, NativeLibrary, Result};
use base64::{engine::general_purpose::URL_SAFE_NO_PAD, Engine};
use serde::Deserialize;
use serde_json::json;
use std::{
    collections::BTreeMap,
    fmt,
    sync::{Arc, RwLock},
};

pub type KeyProvider = dyn Fn(&str) -> CallbackResult<Option<String>> + Send + Sync;
pub type WebhookCallback = dyn Fn(&WebhookEvent) -> CallbackResult<()> + Send + Sync;

#[derive(Clone, Debug, Deserialize, PartialEq, Eq)]
pub struct WebhookRoom {
    pub sid: String,
    pub name: String,
    pub metadata: String,
}
#[derive(Clone, Debug, Deserialize, PartialEq, Eq)]
pub struct WebhookParticipant {
    pub sid: String,
    pub identity: String,
    pub name: String,
    pub metadata: String,
}
#[derive(Clone, Debug, Deserialize, PartialEq, Eq)]
pub struct WebhookTrack {
    pub sid: String,
    pub name: String,
}
#[derive(Clone, Debug, Deserialize, PartialEq, Eq)]
pub struct WebhookEgress {
    pub egress_id: String,
    pub room_id: String,
    pub room_name: String,
}
#[derive(Clone, Debug, Deserialize, PartialEq, Eq)]
pub struct WebhookIngress {
    pub ingress_id: String,
    pub name: String,
    pub room_name: String,
}

#[derive(Clone, Debug, Deserialize, PartialEq, Eq)]
pub struct WebhookEvent {
    pub event: String,
    pub id: String,
    #[serde(deserialize_with = "decimal_i64")]
    pub created_at: i64,
    pub raw_body: String,
    pub room: Option<WebhookRoom>,
    pub participant: Option<WebhookParticipant>,
    pub track: Option<WebhookTrack>,
    pub egress: Option<WebhookEgress>,
    pub ingress: Option<WebhookIngress>,
}

fn decimal_i64<'de, D: serde::Deserializer<'de>>(d: D) -> std::result::Result<i64, D::Error> {
    String::deserialize(d)?
        .parse()
        .map_err(serde::de::Error::custom)
}

#[derive(Clone)]
enum Keys {
    Secrets(BTreeMap<String, String>),
    Provider(Arc<KeyProvider>),
}

/// Verification precedes delivery. Clones share callback replacement; no user code runs under locks.
#[derive(Clone)]
pub struct WebhookReceiver {
    native: NativeLibrary,
    keys: Keys,
    callback: Arc<RwLock<Option<Arc<WebhookCallback>>>>,
}

impl fmt::Debug for WebhookReceiver {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("WebhookReceiver")
            .field("credentials", &"[redacted]")
            .finish_non_exhaustive()
    }
}

impl WebhookReceiver {
    pub fn new(
        native: NativeLibrary,
        api_key: impl Into<String>,
        api_secret: impl Into<String>,
    ) -> Self {
        Self::with_secrets(
            native,
            BTreeMap::from([(api_key.into(), api_secret.into())]),
        )
    }
    pub fn with_secrets(native: NativeLibrary, secrets: BTreeMap<String, String>) -> Self {
        Self {
            native,
            keys: Keys::Secrets(secrets),
            callback: Arc::default(),
        }
    }
    pub fn with_key_provider(native: NativeLibrary, provider: Arc<KeyProvider>) -> Self {
        Self {
            native,
            keys: Keys::Provider(provider),
            callback: Arc::default(),
        }
    }
    pub fn set_callback(&self, callback: Option<Arc<WebhookCallback>>) -> Result<()> {
        let old = {
            let mut guard = self
                .callback
                .write()
                .map_err(|_| Error::Binding("Webhook callback lock poisoned".into()))?;
            std::mem::replace(&mut *guard, callback)
        };
        // Dropping captured values can execute application destructors; do so outside the lock.
        drop(old);
        Ok(())
    }
    pub fn receive(&self, body: impl AsRef<[u8]>, authorization: &str) -> Result<WebhookEvent> {
        let (secrets, provider) = match &self.keys {
            Keys::Secrets(secrets) => (json!(secrets), None),
            Keys::Provider(provider) => (json!({}), Some(provider.as_ref())),
        };
        let value = self.native.invoke(
            json!({"operation": "webhook", "options": {
            "body": URL_SAFE_NO_PAD.encode(body.as_ref()), "authorization": authorization,
            "secrets": secrets, "use_key_provider": provider.is_some() }}),
            None,
            provider,
        )?;
        Ok(serde_json::from_value(value)?)
    }
    pub fn dispatch(&self, event: &WebhookEvent) -> Result<()> {
        let callback = self
            .callback
            .read()
            .map_err(|_| Error::Binding("Webhook callback lock poisoned".into()))?
            .clone();
        if let Some(callback) = callback {
            callback(event).map_err(Error::Callback)?;
        }
        Ok(())
    }
    pub fn receive_and_dispatch(
        &self,
        body: impl AsRef<[u8]>,
        authorization: &str,
    ) -> Result<WebhookEvent> {
        let event = self.receive(body, authorization)?;
        self.dispatch(&event)?;
        Ok(event)
    }
}
