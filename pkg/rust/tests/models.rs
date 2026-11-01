use livekit_server_cpp::{models::*, *};
use serde_json::json;

#[test]
fn detached_models_serde_and_invalid_objects() {
    let input = json!({"nested": {"name":"中文😀\0"}, "large":"9223372036854775807", "empty":{}});
    let model = Room::from_value(input.clone()).unwrap();
    let mut copy = model.to_value().unwrap();
    copy["nested"]["name"] = json!("changed");
    assert_eq!(model.to_value().unwrap(), input);
    assert_eq!(serde_json::to_value(&model).unwrap(), input);
    assert_ne!(
        std::any::TypeId::of::<Room>(),
        std::any::TypeId::of::<CreateRoomRequest>()
    );
    for invalid in ["[]", "null", "false", "not JSON"] {
        assert!(Room::from_json(invalid).to_value().is_err());
        assert!(serde_json::from_str::<Room>(invalid).is_err());
    }
    assert!(Room::from_value(json!([])).is_err());
}

#[test]
fn optional_grants_and_redacted_options() {
    let grant = VideoGrant {
        room_join: true,
        can_publish: Some(false),
        ..Default::default()
    };
    let fields = serde_json::to_value(&grant).unwrap();
    assert_eq!(fields["can_publish"], false);
    assert!(fields.get("can_subscribe").is_none());
    assert_eq!(serde_json::from_value::<VideoGrant>(fields).unwrap(), grant);
    assert!(serde_json::from_value::<VideoGrant>(json!({"can_publish":1})).is_err());
    assert_eq!(
        serde_json::to_value(SipGrant {
            admin: true,
            call: false
        })
        .unwrap(),
        json!({"admin":true,"call":false})
    );
    assert_eq!(
        serde_json::to_value(AgentGrant::default()).unwrap(),
        json!({"admin":false,"simulation_admin":false})
    );
    let options = ApiOptions {
        api_secret: "secret-should-not-appear".into(),
        ..Default::default()
    };
    assert!(!format!("{options:?}").contains("secret-should-not-appear"));
    assert_eq!(options.timeout_ms, 30_000);
}
