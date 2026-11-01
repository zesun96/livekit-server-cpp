//! Synchronous server SDK over public C ABI 1. See the package README for native deployment.
//!
//! Loading a trusted, compatible shared SDK is the only unsafe public operation. Models,
//! services, token signing and webhook verification then use safe Rust APIs.
#![deny(unsafe_op_in_unsafe_fn)]

mod api;
mod error;
mod grants;
pub mod models;
mod native;
mod services;
mod token;
mod webhook;

pub use api::{ApiOptions, HttpRequest, HttpResponse, LiveKitApi, Transport};
pub use error::{CallbackError, CallbackResult, Error, Result};
pub use grants::{AgentGrant, SipGrant, VideoGrant};
pub use native::NativeLibrary;
pub use serde_json;
pub use services::*;
pub use token::AccessToken;
pub use webhook::*;
