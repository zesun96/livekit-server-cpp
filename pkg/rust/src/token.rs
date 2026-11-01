use crate::{AgentGrant, Error, NativeLibrary, Result, SipGrant, VideoGrant};
use serde_json::{json, Map, Value};
use std::{
    collections::BTreeMap,
    fmt,
    time::{SystemTime, UNIX_EPOCH},
};

/// Builder that owns snapshots of credentials, attributes and grants. Signing stays in C++.
#[derive(Clone)]
pub struct AccessToken {
    native: NativeLibrary,
    fields: Map<String, Value>,
}

impl fmt::Debug for AccessToken {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("AccessToken")
            .field("credentials", &"[redacted]")
            .finish_non_exhaustive()
    }
}

impl AccessToken {
    pub fn new(
        native: NativeLibrary,
        api_key: impl Into<String>,
        api_secret: impl Into<String>,
    ) -> Self {
        let mut fields = Map::new();
        fields.insert("api_key".into(), Value::String(api_key.into()));
        fields.insert("api_secret".into(), Value::String(api_secret.into()));
        fields.insert("valid_for".into(), Value::String("21600".into()));
        Self { native, fields }
    }
    fn field(mut self, key: &str, value: Value) -> Self {
        self.fields.insert(key.into(), value);
        self
    }
    pub fn with_identity(self, value: impl Into<String>) -> Self {
        self.field("identity", Value::String(value.into()))
    }
    pub fn with_name(self, value: impl Into<String>) -> Self {
        self.field("name", Value::String(value.into()))
    }
    pub fn with_metadata(self, value: impl Into<String>) -> Self {
        self.field("metadata", Value::String(value.into()))
    }
    pub fn with_attributes(self, value: BTreeMap<String, String>) -> Self {
        self.field("attributes", json!(value))
    }
    pub fn with_valid_for(self, seconds: i64) -> Result<Self> {
        if seconds <= 0 {
            return Err(Error::InvalidArgument(
                "Token lifetime must be positive seconds".into(),
            ));
        }
        Ok(self.field("valid_for", Value::String(seconds.to_string())))
    }
    pub fn with_video_grant(self, grant: VideoGrant) -> Self {
        self.field("video", json!(grant))
    }
    pub fn with_sip_grant(self, grant: SipGrant) -> Self {
        self.field("sip", json!(grant))
    }
    pub fn with_agent_grant(self, grant: AgentGrant) -> Self {
        self.field("agent", json!(grant))
    }
    pub fn to_jwt(&self) -> Result<String> {
        self.to_jwt_at_time(SystemTime::now())
    }
    pub fn to_jwt_at_time(&self, now: SystemTime) -> Result<String> {
        let duration = now
            .duration_since(UNIX_EPOCH)
            .map_err(|_| Error::InvalidArgument("Time precedes Unix epoch".into()))?;
        let seconds = i64::try_from(duration.as_secs())
            .map_err(|_| Error::InvalidArgument("Time exceeds signed 64-bit seconds".into()))?;
        self.to_jwt_at(seconds)
    }
    /// Explicit Unix seconds; native code validates time and expiry overflow.
    pub fn to_jwt_at(&self, now: i64) -> Result<String> {
        let mut fields = self.fields.clone();
        fields.insert("now".into(), Value::String(now.to_string()));
        let value =
            self.native
                .invoke(json!({"operation": "token", "options": fields}), None, None)?;
        value
            .as_str()
            .map(str::to_owned)
            .ok_or_else(|| Error::Binding("Expected JWT string".into()))
    }
}
