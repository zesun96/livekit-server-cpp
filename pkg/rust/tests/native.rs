//! Offline native C ABI contract tests. Explicitly run ignored tests with a trusted SDK path.
use base64::{
    engine::general_purpose::{STANDARD, URL_SAFE_NO_PAD},
    Engine,
};
use hmac::{Hmac, Mac};
use livekit_server_cpp::{models as m, *};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::{
    cell::Cell,
    collections::BTreeMap,
    panic::{self, AssertUnwindSafe},
    sync::{Arc, Mutex},
    thread,
    time::{SystemTime, UNIX_EPOCH},
};

const KEY: &str = "rust-test-key";
const SECRET: &str = "rust-test-secret-at-least-32-characters";

fn native() -> NativeLibrary {
    let path = std::env::var_os("LIVEKIT_SERVER_RUST_LIBRARY")
        .expect("Set trusted LIVEKIT_SERVER_RUST_LIBRARY to run native tests");
    // SAFETY: this opt-in test is run with a trusted workspace-built ABI 1 SDK and dependencies.
    unsafe { NativeLibrary::load(path).unwrap() }
}

fn api(native: &NativeLibrary, transport: Arc<Transport>) -> LiveKitApi {
    LiveKitApi::new(
        native.clone(),
        ApiOptions {
            url: "ws://offline.invalid/".into(),
            api_key: KEY.into(),
            api_secret: SECRET.into(),
            timeout_ms: 1234,
            transport: Some(transport),
            ..Default::default()
        },
    )
    .unwrap()
}

fn response(status_code: i32, body: impl Into<Vec<u8>>) -> CallbackResult<HttpResponse> {
    Ok(HttpResponse {
        status_code,
        body: body.into(),
    })
}

fn sign(unsigned: &str) -> Vec<u8> {
    let mut mac = Hmac::<Sha256>::new_from_slice(SECRET.as_bytes()).unwrap();
    mac.update(unsigned.as_bytes());
    mac.finalize().into_bytes().to_vec()
}

fn authorization(body: &[u8], changes: Value) -> String {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let mut claims = json!({"iss": KEY, "nbf": now - 60, "exp": now + 3600,
        "sha256": STANDARD.encode(Sha256::digest(body))});
    for (key, value) in changes.as_object().unwrap() {
        claims[key] = value.clone();
    }
    let unsigned = format!(
        "{}.{}",
        URL_SAFE_NO_PAD.encode(br#"{"alg":"HS256","typ":"JWT"}"#),
        URL_SAFE_NO_PAD.encode(serde_json::to_vec(&claims).unwrap())
    );
    format!("{}.{}", unsigned, URL_SAFE_NO_PAD.encode(sign(&unsigned)))
}

fn category(error: Error, expected: i32) -> Error {
    assert!(
        matches!(&error, Error::Native {code, ..} if *code == expected),
        "{error:?}"
    );
    error
}

#[test]
#[ignore = "requires trusted LIVEKIT_SERVER_RUST_LIBRARY; in-memory HTTP only"]
fn all_service_routes() {
    let native = native();
    macro_rules! route {
        ($service:ident, $method:ident, $request:ident, $rpc:literal) => {{
            let api =
                api(
                    &native,
                    Arc::new(|request| {
                        assert_eq!(
                            request.url,
                            concat!("http://offline.invalid/twirp/livekit.", $rpc)
                        );
                        assert_eq!(request.method, "POST");
                        assert_eq!(request.timeout_ms, 1234);
                        assert!(request
                            .headers
                            .contains(&("Content-Type".into(), "application/protobuf".into())));
                        assert!(request
                            .headers
                            .iter()
                            .any(|(key, value)| key == "Authorization"
                                && value.starts_with("Bearer ")));
                        response(200, [])
                    }),
                );
            let result = api.$service().$method(&m::$request::default()).unwrap();
            assert_eq!(result.to_value().unwrap(), json!({}));
        }};
    }
    // Independent protocol fixtures, not derived from the generated Rust method table.
    route!(
        room,
        create_room,
        CreateRoomRequest,
        "RoomService/CreateRoom"
    );
    route!(room, list_rooms, ListRoomsRequest, "RoomService/ListRooms");
    route!(
        room,
        delete_room,
        DeleteRoomRequest,
        "RoomService/DeleteRoom"
    );
    route!(
        room,
        list_participants,
        ListParticipantsRequest,
        "RoomService/ListParticipants"
    );
    route!(
        room,
        get_participant,
        RoomParticipantIdentity,
        "RoomService/GetParticipant"
    );
    route!(
        room,
        remove_participant,
        RoomParticipantIdentity,
        "RoomService/RemoveParticipant"
    );
    route!(
        room,
        mute_published_track,
        MuteRoomTrackRequest,
        "RoomService/MutePublishedTrack"
    );
    route!(
        room,
        update_participant,
        UpdateParticipantRequest,
        "RoomService/UpdateParticipant"
    );
    route!(
        room,
        update_subscriptions,
        UpdateSubscriptionsRequest,
        "RoomService/UpdateSubscriptions"
    );
    route!(room, send_data, SendDataRequest, "RoomService/SendData");
    route!(
        room,
        update_room_metadata,
        UpdateRoomMetadataRequest,
        "RoomService/UpdateRoomMetadata"
    );
    route!(
        room,
        forward_participant,
        ForwardParticipantRequest,
        "RoomService/ForwardParticipant"
    );
    route!(
        room,
        move_participant,
        MoveParticipantRequest,
        "RoomService/MoveParticipant"
    );
    route!(
        room,
        perform_rpc,
        PerformRpcRequest,
        "RoomService/PerformRpc"
    );
    route!(
        egress,
        start_egress,
        StartEgressRequest,
        "Egress/StartEgress"
    );
    route!(
        egress,
        start_room_composite_egress,
        RoomCompositeEgressRequest,
        "Egress/StartRoomCompositeEgress"
    );
    route!(
        egress,
        start_web_egress,
        WebEgressRequest,
        "Egress/StartWebEgress"
    );
    route!(
        egress,
        start_participant_egress,
        ParticipantEgressRequest,
        "Egress/StartParticipantEgress"
    );
    route!(
        egress,
        start_track_composite_egress,
        TrackCompositeEgressRequest,
        "Egress/StartTrackCompositeEgress"
    );
    route!(
        egress,
        start_track_egress,
        TrackEgressRequest,
        "Egress/StartTrackEgress"
    );
    route!(
        egress,
        update_layout,
        UpdateLayoutRequest,
        "Egress/UpdateLayout"
    );
    route!(
        egress,
        update_stream,
        UpdateStreamRequest,
        "Egress/UpdateStream"
    );
    route!(egress, list_egress, ListEgressRequest, "Egress/ListEgress");
    route!(egress, stop_egress, StopEgressRequest, "Egress/StopEgress");
    route!(
        ingress,
        create_ingress,
        CreateIngressRequest,
        "Ingress/CreateIngress"
    );
    route!(
        ingress,
        update_ingress,
        UpdateIngressRequest,
        "Ingress/UpdateIngress"
    );
    route!(
        ingress,
        list_ingress,
        ListIngressRequest,
        "Ingress/ListIngress"
    );
    route!(
        ingress,
        delete_ingress,
        DeleteIngressRequest,
        "Ingress/DeleteIngress"
    );
    route!(
        sip,
        create_inbound_trunk,
        CreateSIPInboundTrunkRequest,
        "SIP/CreateSIPInboundTrunk"
    );
    route!(
        sip,
        create_outbound_trunk,
        CreateSIPOutboundTrunkRequest,
        "SIP/CreateSIPOutboundTrunk"
    );
    route!(
        sip,
        update_inbound_trunk,
        UpdateSIPInboundTrunkRequest,
        "SIP/UpdateSIPInboundTrunk"
    );
    route!(
        sip,
        update_outbound_trunk,
        UpdateSIPOutboundTrunkRequest,
        "SIP/UpdateSIPOutboundTrunk"
    );
    route!(
        sip,
        get_inbound_trunk,
        GetSIPInboundTrunkRequest,
        "SIP/GetSIPInboundTrunk"
    );
    route!(
        sip,
        get_outbound_trunk,
        GetSIPOutboundTrunkRequest,
        "SIP/GetSIPOutboundTrunk"
    );
    route!(sip, list_trunks, ListSIPTrunkRequest, "SIP/ListSIPTrunk");
    route!(
        sip,
        list_inbound_trunks,
        ListSIPInboundTrunkRequest,
        "SIP/ListSIPInboundTrunk"
    );
    route!(
        sip,
        list_outbound_trunks,
        ListSIPOutboundTrunkRequest,
        "SIP/ListSIPOutboundTrunk"
    );
    route!(
        sip,
        delete_trunk,
        DeleteSIPTrunkRequest,
        "SIP/DeleteSIPTrunk"
    );
    route!(
        sip,
        create_dispatch_rule,
        CreateSIPDispatchRuleRequest,
        "SIP/CreateSIPDispatchRule"
    );
    route!(
        sip,
        update_dispatch_rule,
        UpdateSIPDispatchRuleRequest,
        "SIP/UpdateSIPDispatchRule"
    );
    route!(
        sip,
        list_dispatch_rules,
        ListSIPDispatchRuleRequest,
        "SIP/ListSIPDispatchRule"
    );
    route!(
        sip,
        delete_dispatch_rule,
        DeleteSIPDispatchRuleRequest,
        "SIP/DeleteSIPDispatchRule"
    );
    route!(
        sip,
        create_participant,
        CreateSIPParticipantRequest,
        "SIP/CreateSIPParticipant"
    );
    route!(
        sip,
        transfer_participant,
        TransferSIPParticipantRequest,
        "SIP/TransferSIPParticipant"
    );
    route!(
        agent_dispatch,
        create_dispatch,
        CreateAgentDispatchRequest,
        "AgentDispatchService/CreateDispatch"
    );
    route!(
        agent_dispatch,
        delete_dispatch,
        DeleteAgentDispatchRequest,
        "AgentDispatchService/DeleteDispatch"
    );
    route!(
        agent_dispatch,
        list_dispatch,
        ListAgentDispatchRequest,
        "AgentDispatchService/ListDispatch"
    );
    route!(
        connector,
        dial_whats_app_call,
        DialWhatsAppCallRequest,
        "Connector/DialWhatsAppCall"
    );
    route!(
        connector,
        disconnect_whats_app_call,
        DisconnectWhatsAppCallRequest,
        "Connector/DisconnectWhatsAppCall"
    );
    route!(
        connector,
        connect_whats_app_call,
        ConnectWhatsAppCallRequest,
        "Connector/ConnectWhatsAppCall"
    );
    route!(
        connector,
        accept_whats_app_call,
        AcceptWhatsAppCallRequest,
        "Connector/AcceptWhatsAppCall"
    );
    route!(
        connector,
        connect_twilio_call,
        ConnectTwilioCallRequest,
        "Connector/ConnectTwilioCall"
    );
}

#[test]
#[ignore = "requires trusted LIVEKIT_SERVER_RUST_LIBRARY; in-memory HTTP only"]
fn binary_unicode_and_dispatch() {
    let native = native();
    let name = "中文😀\0";
    let api = api(
        &native,
        Arc::new(move |request| {
            assert_eq!(
                request.body,
                [vec![10, name.len() as u8], name.as_bytes().to_vec()].concat()
            );
            response(
                200,
                [vec![18, name.len() as u8], name.as_bytes().to_vec()].concat(),
            )
        }),
    );
    assert_eq!(
        api.room()
            .create_room(&m::CreateRoomRequest::from_value(json!({"name":name})).unwrap())
            .unwrap()
            .to_value()
            .unwrap()["name"],
        name
    );
    let api = LiveKitApi::new(
        native.clone(),
        ApiOptions {
            url: "https://offline.invalid".into(),
            access_token: "preissued-test-token".into(),
            transport: Some(Arc::new(|request| {
                assert!(request
                    .headers
                    .contains(&("Authorization".into(), "Bearer preissued-test-token".into())));
                response(200, b"\x0a\x06\x0a\x04AD_1")
            })),
            ..Default::default()
        },
    )
    .unwrap();
    assert_eq!(
        api.agent_dispatch()
            .get_dispatch("AD_1", "room")
            .unwrap()
            .unwrap()
            .to_value()
            .unwrap()["id"],
        "AD_1"
    );
    let empty = self::api(&native, Arc::new(|_| response(200, [])));
    assert!(empty
        .agent_dispatch()
        .get_dispatch("missing", "room")
        .unwrap()
        .is_none());
}

#[test]
#[ignore = "requires trusted LIVEKIT_SERVER_RUST_LIBRARY; no network"]
fn independent_token_signature_and_grants() {
    let native = native();
    let token = AccessToken::new(native.clone(), KEY, SECRET)
        .with_identity("alice😀\0")
        .with_name("name")
        .with_metadata("quote\"\n\0")
        .with_attributes(BTreeMap::from([("语言".into(), "中文😀".into())]))
        .with_valid_for(60)
        .unwrap()
        .with_video_grant(VideoGrant {
            room_join: true,
            room: "room".into(),
            can_publish: Some(false),
            can_publish_sources: vec!["microphone".into()],
            ..Default::default()
        })
        .with_sip_grant(SipGrant {
            call: true,
            ..Default::default()
        })
        .with_agent_grant(AgentGrant {
            simulation_admin: true,
            ..Default::default()
        });
    let jwt = token.to_jwt_at(1700000000).unwrap();
    let parts: Vec<_> = jwt.split('.').collect();
    assert_eq!(
        URL_SAFE_NO_PAD.decode(parts[2]).unwrap(),
        sign(&format!("{}.{}", parts[0], parts[1]))
    );
    let claims: Value = serde_json::from_slice(&URL_SAFE_NO_PAD.decode(parts[1]).unwrap()).unwrap();
    assert_eq!(claims["sub"], "alice😀\0");
    assert_eq!(claims["metadata"], "quote\"\n\0");
    assert_eq!(claims["attributes"]["语言"], "中文😀");
    assert_eq!(claims["video"]["canPublish"], false);
    assert!(claims["video"].get("canSubscribe").is_none());
    assert_eq!(claims["video"]["canPublishSources"], json!(["microphone"]));
    assert_eq!(claims["sip"]["call"], true);
    assert_eq!(claims["agent"]["simulationAdmin"], true);
    assert_eq!(claims["nbf"], 1700000000);
    assert_eq!(claims["exp"], 1700000060);
    assert_eq!(
        token
            .to_jwt_at_time(UNIX_EPOCH + std::time::Duration::from_secs(1700000000))
            .unwrap(),
        jwt
    );
    assert!(token
        .to_jwt_at_time(UNIX_EPOCH - std::time::Duration::from_millis(1))
        .is_err());
    assert!(!format!("{token:?}").contains(SECRET));
    category(token.to_jwt_at(-1).unwrap_err(), Error::INVALID_ARGUMENT);
    category(
        token
            .with_valid_for(i64::MAX)
            .unwrap()
            .to_jwt_at(1700000000)
            .unwrap_err(),
        Error::INVALID_ARGUMENT,
    );
    category(
        AccessToken::new(native, "", "")
            .to_jwt_at(1700000000)
            .unwrap_err(),
        Error::AUTHENTICATION,
    );
}

#[test]
#[ignore = "requires trusted LIVEKIT_SERVER_RUST_LIBRARY; in-memory HTTP only"]
fn errors_and_original_callback_source() {
    let native = native();
    let client = api(&native, Arc::new(|_| response(200, [])));
    for json in ["not JSON", r#"{"unknown":true}"#] {
        category(
            client
                .room()
                .create_room(&m::CreateRoomRequest::from_json(json))
                .unwrap_err(),
            Error::PROTOCOL,
        );
    }
    category(
        api(&native, Arc::new(|_| response(200, [255])))
            .room()
            .list_rooms(&Default::default())
            .unwrap_err(),
        Error::PROTOCOL,
    );
    category(
        api(&native, Arc::new(|_| response(999, [])))
            .room()
            .list_rooms(&Default::default())
            .unwrap_err(),
        Error::TRANSPORT,
    );
    let error = api(
        &native,
        Arc::new(|_| {
            response(
                403,
                serde_json::to_vec(&json!({"code":"permission_denied","msg":"denied 中文😀"}))
                    .unwrap(),
            )
        }),
    )
    .room()
    .list_rooms(&Default::default())
    .unwrap_err();
    match error {
        Error::Native {
            code,
            message,
            http_status,
            twirp_code,
        } => {
            assert_eq!(
                (code, message.as_str(), http_status, twirp_code.as_str()),
                (Error::HTTP, "denied 中文😀", 403, "permission_denied")
            );
        }
        other => panic!("{other:?}"),
    }
    let marker = Arc::new(());
    let captured = marker.clone();
    let client = api(
        &native,
        Arc::new(move |_| Err(Box::new(Sentinel(captured.clone())))),
    );
    match client.room().list_rooms(&Default::default()).unwrap_err() {
        Error::Callback(error) => assert!(Arc::ptr_eq(
            &error.downcast_ref::<Sentinel>().unwrap().0,
            &marker
        )),
        other => panic!("{other:?}"),
    }
}

#[derive(Debug)]
struct Sentinel(Arc<()>);
impl std::fmt::Display for Sentinel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("host sentinel")
    }
}
impl std::error::Error for Sentinel {}

#[test]
#[ignore = "requires trusted LIVEKIT_SERVER_RUST_LIBRARY; panic=unwind"]
fn callback_panic_resumes_original_payload_and_recovers() {
    let native = native();
    let marker = Arc::new(());
    let captured = marker.clone();
    let client = api(
        &native,
        Arc::new(move |_| panic::panic_any(captured.clone())),
    );
    let payload = panic::catch_unwind(AssertUnwindSafe(|| {
        client.room().list_rooms(&Default::default())
    }))
    .unwrap_err();
    assert!(Arc::ptr_eq(
        payload.downcast_ref::<Arc<()>>().unwrap(),
        &marker
    ));
    api(&native, Arc::new(|_| response(200, [])))
        .room()
        .list_rooms(&Default::default())
        .unwrap();
}

thread_local! { static CALLER: Cell<Option<thread::ThreadId>> = const { Cell::new(None) }; }

#[test]
#[ignore = "requires trusted LIVEKIT_SERVER_RUST_LIBRARY; nested/concurrent offline callbacks"]
fn concurrent_nested_calls_and_module_ownership() {
    fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<NativeLibrary>();
    assert_send_sync::<LiveKitApi>();
    assert_send_sync::<WebhookReceiver>();
    let native = native();
    let inner = api(&native, Arc::new(|_| response(200, [])));
    let token = AccessToken::new(native.clone(), KEY, SECRET);
    let client = api(
        &native,
        Arc::new(move |_| {
            CALLER.with(|id| assert_eq!(id.get(), Some(thread::current().id())));
            inner.room().list_rooms(&Default::default()).unwrap();
            assert!(!token.to_jwt_at(1700000000).unwrap().is_empty());
            response(200, b"\x12\x04room")
        }),
    );
    drop(native);
    let threads: Vec<_> = (0..8)
        .map(|_| {
            let client = client.clone();
            thread::spawn(move || {
                CALLER.with(|id| id.set(Some(thread::current().id())));
                for _ in 0..10 {
                    assert_eq!(
                        client
                            .room()
                            .create_room(&Default::default())
                            .unwrap()
                            .to_value()
                            .unwrap()["name"],
                        "room"
                    );
                }
            })
        })
        .collect();
    drop(client);
    for thread in threads {
        thread.join().unwrap();
    }
}

fn webhook_body() -> Vec<u8> {
    serde_json::to_vec(
        &json!({"event":"participant_joined","id":"EV_1","createdAt":"1700000000",
        "room":{"sid":"RM_1","name":"room","metadata":"r"},
        "participant":{"sid":"PA_1","identity":"alice","name":"A","metadata":"p"},
        "track":{"sid":"TR_1","name":"microphone"},
        "egressInfo":{"egressId":"EG_1","roomId":"RM_1","roomName":"room"},
        "ingressInfo":{"ingressId":"IN_1","name":"input","roomName":"room"},
        "futureField":{"中文😀":true}}),
    )
    .unwrap()
}

#[test]
#[ignore = "requires trusted LIVEKIT_SERVER_RUST_LIBRARY; independently signed webhook"]
fn webhook_snapshots_raw_body_and_tampering() {
    let native = native();
    let body = webhook_body();
    let seen = Arc::new(Mutex::new(Vec::new()));
    let receiver = WebhookReceiver::with_secrets(
        native,
        BTreeMap::from([
            (KEY.into(), SECRET.into()),
            ("other".into(), "unused".into()),
        ]),
    );
    let capture = seen.clone();
    receiver
        .set_callback(Some(Arc::new(move |event| {
            capture.lock().unwrap().push(event.id.clone());
            Ok(())
        })))
        .unwrap();
    let auth = authorization(&body, json!({}));
    let event = receiver
        .receive_and_dispatch(&body, &format!("Bearer {auth}"))
        .unwrap();
    assert_eq!(&*seen.lock().unwrap(), &["EV_1"]);
    assert_eq!(event.raw_body.as_bytes(), body);
    assert_eq!(event.created_at, 1700000000);
    assert_eq!(event.room.unwrap().sid, "RM_1");
    assert_eq!(event.participant.unwrap().identity, "alice");
    assert_eq!(event.track.unwrap().sid, "TR_1");
    assert_eq!(event.egress.unwrap().egress_id, "EG_1");
    assert_eq!(event.ingress.unwrap().ingress_id, "IN_1");
    seen.lock().unwrap().clear();
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs();
    for (bytes, jwt) in [
        ([body.clone(), vec![32]].concat(), auth.clone()),
        (body.clone(), authorization(&body, json!({"iss":"unknown"}))),
        (
            body.clone(),
            authorization(&body, json!({"nbf": now-7200,"exp":now-3600})),
        ),
        (
            body.clone(),
            authorization(&body, json!({"nbf": now+3600,"exp":now+7200})),
        ),
        (
            body.clone(),
            format!(
                "{}.{}",
                auth.rsplit_once('.').unwrap().0,
                URL_SAFE_NO_PAD.encode([0; 32])
            ),
        ),
    ] {
        category(
            receiver.receive_and_dispatch(bytes, &jwt).unwrap_err(),
            Error::AUTHENTICATION,
        );
    }
    assert!(seen.lock().unwrap().is_empty());
}

#[test]
#[ignore = "requires trusted LIVEKIT_SERVER_RUST_LIBRARY; nested key callbacks"]
fn dynamic_keys_errors_and_panics() {
    let native = native();
    let inner = api(&native, Arc::new(|_| response(200, [])));
    let receiver = WebhookReceiver::with_key_provider(
        native.clone(),
        Arc::new(move |key| {
            inner.room().list_rooms(&Default::default()).unwrap();
            Ok((key == KEY).then(|| SECRET.to_string()))
        }),
    );
    let body = webhook_body();
    let auth = authorization(&body, json!({}));
    assert_eq!(receiver.receive(&body, &auth).unwrap().id, "EV_1");
    category(
        receiver
            .receive(&body, &authorization(&body, json!({"iss":"unknown"})))
            .unwrap_err(),
        Error::AUTHENTICATION,
    );
    let marker = Arc::new(());
    let capture = marker.clone();
    let receiver = WebhookReceiver::with_key_provider(
        native.clone(),
        Arc::new(move |_| Err(Box::new(Sentinel(capture.clone())))),
    );
    assert!(matches!(
        receiver.receive(&body, &auth),
        Err(Error::Callback(_))
    ));
    let capture = marker.clone();
    let receiver = WebhookReceiver::with_key_provider(
        native,
        Arc::new(move |_| panic::panic_any(capture.clone())),
    );
    let payload =
        panic::catch_unwind(AssertUnwindSafe(|| receiver.receive(&body, &auth))).unwrap_err();
    assert!(Arc::ptr_eq(
        payload.downcast_ref::<Arc<()>>().unwrap(),
        &marker
    ));
}

#[test]
#[ignore = "requires trusted LIVEKIT_SERVER_RUST_LIBRARY; reentrant event callbacks"]
fn callback_replacement_dispatch_and_destructors() {
    let native = native();
    let receiver = Arc::new(WebhookReceiver::new(native, KEY, SECRET));
    let body = webhook_body();
    let event = receiver
        .receive(&body, &authorization(&body, json!({})))
        .unwrap();
    let seen = Arc::new(Mutex::new(0));
    let weak = Arc::downgrade(&receiver);
    let capture = seen.clone();
    receiver
        .set_callback(Some(Arc::new(move |event| {
            let receiver = weak.upgrade().unwrap();
            let capture = capture.clone();
            receiver
                .set_callback(Some(Arc::new(move |_| {
                    *capture.lock().unwrap() += 1;
                    Ok(())
                })))
                .unwrap();
            receiver.dispatch(event).unwrap();
            Ok(())
        })))
        .unwrap();
    receiver.dispatch(&event).unwrap();
    assert_eq!(*seen.lock().unwrap(), 1);
    receiver.set_callback(None).unwrap();
    receiver.dispatch(&event).unwrap();
    assert_eq!(*seen.lock().unwrap(), 1);
    struct Reenter(std::sync::Weak<WebhookReceiver>);
    impl Drop for Reenter {
        fn drop(&mut self) {
            self.0.upgrade().unwrap().set_callback(None).unwrap();
        }
    }
    let guard = Reenter(Arc::downgrade(&receiver));
    receiver
        .set_callback(Some(Arc::new(move |_| {
            let _ = &guard;
            Ok(())
        })))
        .unwrap();
    receiver.set_callback(None).unwrap(); // Dropping old closure must happen outside the write lock.
    let marker = Arc::new(());
    let capture = marker.clone();
    receiver
        .set_callback(Some(Arc::new(move |_| {
            Err(Box::new(Sentinel(capture.clone())))
        })))
        .unwrap();
    assert!(matches!(receiver.dispatch(&event), Err(Error::Callback(_))));
}
