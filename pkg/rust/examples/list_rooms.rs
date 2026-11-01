use livekit_server_cpp::{models::ListRoomsRequest, ApiOptions, LiveKitApi, NativeLibrary};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // SAFETY: deploy a trusted ABI 1 SDK and its dependencies; control the environment/loader paths.
    let native = unsafe { NativeLibrary::from_env()? };
    // The native SDK reads LIVEKIT_URL and LIVEKIT_TOKEN or LIVEKIT_API_KEY/LIVEKIT_API_SECRET.
    let api = LiveKitApi::new(native, ApiOptions::default())?;
    println!("{}", api.room().list_rooms(&ListRoomsRequest::default())?);
    Ok(())
}
