use std::{error, fmt};

pub type CallbackError = Box<dyn error::Error + Send + Sync + 'static>;
pub type CallbackResult<T> = std::result::Result<T, CallbackError>;
pub type Result<T> = std::result::Result<T, Error>;

/// SDK categories retain their ABI numeric value, including unknown future categories.
#[derive(Debug)]
pub enum Error {
    Native {
        code: i32,
        message: String,
        http_status: i32,
        twirp_code: String,
    },
    Load(String),
    InvalidArgument(String),
    Binding(String),
    Json(serde_json::Error),
    /// Original application error; use `source()` or downcast the boxed error.
    Callback(CallbackError),
}

impl Error {
    pub const INVALID_ARGUMENT: i32 = 0;
    pub const AUTHENTICATION: i32 = 1;
    pub const TRANSPORT: i32 = 2;
    pub const HTTP: i32 = 3;
    pub const PROTOCOL: i32 = 4;
    pub const UNSUPPORTED: i32 = 5;
    pub const UNKNOWN: i32 = 255;
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Native { message, .. } => f.write_str(message),
            Self::Load(message) | Self::InvalidArgument(message) | Self::Binding(message) => {
                f.write_str(message)
            }
            Self::Json(error) => error.fmt(f),
            Self::Callback(error) => error.fmt(f),
        }
    }
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self {
            Self::Json(error) => Some(error),
            Self::Callback(error) => Some(error.as_ref()),
            _ => None,
        }
    }
}

impl From<serde_json::Error> for Error {
    fn from(error: serde_json::Error) -> Self {
        Self::Json(error)
    }
}
