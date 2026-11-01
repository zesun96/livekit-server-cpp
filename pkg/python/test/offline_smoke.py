"""Offline Python/C ABI contract tests: no network, LiveKit server or physical devices.

Set LIVEKIT_SERVER_PYTHON_LIBRARY to run native tests. Optional Windows dependency directories
are supplied through LIVEKIT_SERVER_PYTHON_DLL_DIRECTORIES (os.pathsep-separated).
"""
from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from dataclasses import FrozenInstanceError
from datetime import datetime, timedelta, timezone
import gc
import base64
import hashlib
import hmac
import json
import os
import subprocess
import sys
import threading
import time
import unittest
from unittest.mock import patch

import livekit_server as S
from livekit_server import models as M
from livekit_server._native import _b64, _unb64

KEY = "python-test-key"
SECRET = "python-test-secret-at-least-32-characters"

# Independent protocol routes, kept explicit so generated service tables cannot validate themselves.
ROUTES = """
room create_room CreateRoomRequest CreateRoom
room list_rooms ListRoomsRequest ListRooms
room delete_room DeleteRoomRequest DeleteRoom
room list_participants ListParticipantsRequest ListParticipants
room get_participant RoomParticipantIdentity GetParticipant
room remove_participant RoomParticipantIdentity RemoveParticipant
room mute_published_track MuteRoomTrackRequest MutePublishedTrack
room update_participant UpdateParticipantRequest UpdateParticipant
room update_subscriptions UpdateSubscriptionsRequest UpdateSubscriptions
room send_data SendDataRequest SendData
room update_room_metadata UpdateRoomMetadataRequest UpdateRoomMetadata
room forward_participant ForwardParticipantRequest ForwardParticipant
room move_participant MoveParticipantRequest MoveParticipant
room perform_rpc PerformRpcRequest PerformRpc
egress start_egress StartEgressRequest StartEgress
egress start_room_composite_egress RoomCompositeEgressRequest StartRoomCompositeEgress
egress start_web_egress WebEgressRequest StartWebEgress
egress start_participant_egress ParticipantEgressRequest StartParticipantEgress
egress start_track_composite_egress TrackCompositeEgressRequest StartTrackCompositeEgress
egress start_track_egress TrackEgressRequest StartTrackEgress
egress update_layout UpdateLayoutRequest UpdateLayout
egress update_stream UpdateStreamRequest UpdateStream
egress list_egress ListEgressRequest ListEgress
egress stop_egress StopEgressRequest StopEgress
ingress create_ingress CreateIngressRequest CreateIngress
ingress update_ingress UpdateIngressRequest UpdateIngress
ingress list_ingress ListIngressRequest ListIngress
ingress delete_ingress DeleteIngressRequest DeleteIngress
sip create_inbound_trunk CreateSIPInboundTrunkRequest CreateSIPInboundTrunk
sip create_outbound_trunk CreateSIPOutboundTrunkRequest CreateSIPOutboundTrunk
sip update_inbound_trunk UpdateSIPInboundTrunkRequest UpdateSIPInboundTrunk
sip update_outbound_trunk UpdateSIPOutboundTrunkRequest UpdateSIPOutboundTrunk
sip get_inbound_trunk GetSIPInboundTrunkRequest GetSIPInboundTrunk
sip get_outbound_trunk GetSIPOutboundTrunkRequest GetSIPOutboundTrunk
sip list_trunks ListSIPTrunkRequest ListSIPTrunk
sip list_inbound_trunks ListSIPInboundTrunkRequest ListSIPInboundTrunk
sip list_outbound_trunks ListSIPOutboundTrunkRequest ListSIPOutboundTrunk
sip delete_trunk DeleteSIPTrunkRequest DeleteSIPTrunk
sip create_dispatch_rule CreateSIPDispatchRuleRequest CreateSIPDispatchRule
sip update_dispatch_rule UpdateSIPDispatchRuleRequest UpdateSIPDispatchRule
sip list_dispatch_rules ListSIPDispatchRuleRequest ListSIPDispatchRule
sip delete_dispatch_rule DeleteSIPDispatchRuleRequest DeleteSIPDispatchRule
sip create_participant CreateSIPParticipantRequest CreateSIPParticipant
sip transfer_participant TransferSIPParticipantRequest TransferSIPParticipant
agent_dispatch create_dispatch CreateAgentDispatchRequest CreateDispatch
agent_dispatch delete_dispatch DeleteAgentDispatchRequest DeleteDispatch
agent_dispatch list_dispatch ListAgentDispatchRequest ListDispatch
connector dial_whats_app_call DialWhatsAppCallRequest DialWhatsAppCall
connector disconnect_whats_app_call DisconnectWhatsAppCallRequest DisconnectWhatsAppCall
connector connect_whats_app_call ConnectWhatsAppCallRequest ConnectWhatsAppCall
connector accept_whats_app_call AcceptWhatsAppCallRequest AcceptWhatsAppCall
connector connect_twilio_call ConnectTwilioCallRequest ConnectTwilioCall
""".strip().splitlines()


def authorization(body: bytes, **changes) -> str:
    now = int(time.time())
    claims = {"iss": KEY, "nbf": now - 60, "exp": now + 3600,
              "sha256": base64.b64encode(hashlib.sha256(body).digest()).decode()}
    claims.update(changes)
    unsigned = _b64(b'{"alg":"HS256","typ":"JWT"}') + "." + _b64(json.dumps(claims).encode())
    return unsigned + "." + _b64(hmac.digest(SECRET.encode(), unsigned.encode(), "sha256"))


class PythonContractTests(unittest.TestCase):
    def test_models_are_distinct_immutable_snapshots(self):
        self.assertEqual(len(M.__all__), 86)
        source = {"name": "room", "nested": [1, 2]}
        request = M.CreateRoomRequest(source)
        source["nested"].append(3)
        result = request.to_dict()
        result["nested"].append(4)
        self.assertEqual(request.to_dict()["nested"], [1, 2])
        self.assertEqual(request, M.CreateRoomRequest.from_json(request.json))
        self.assertEqual(hash(request), hash(M.CreateRoomRequest(request.json)))
        self.assertNotEqual(M.Room(), M.CreateRoomRequest())
        with self.assertRaises(FrozenInstanceError):
            request.json = "{}"
        with self.assertRaises(TypeError):
            M.Room([], name="no")
        with self.assertRaises(ValueError):
            M.Room({"value": float("nan")})
        with self.assertRaises(ValueError):
            M.Room("[]").to_dict()

    def test_grants_validate_types_and_copy_sources(self):
        sources = ["microphone"]
        grant = S.VideoGrant(room_join=True, can_publish=False, can_publish_sources=sources)
        sources.append("camera")
        self.assertEqual(grant.can_publish_sources, ("microphone",))
        self.assertFalse(grant.to_dict()["can_publish"])
        self.assertNotIn("can_subscribe", grant.to_dict())
        for args in ({"can_publish": 1}, {"room": None}, {"can_publish_sources": "microphone"},
                     {"room_join": "true"}, {"unknown": True}):
            with self.subTest(args=args), self.assertRaises(TypeError):
                S.VideoGrant(**args)
        with self.assertRaises(FrozenInstanceError):
            grant.room = "new"

    def test_python_validation_and_credential_redaction(self):
        self.assertNotIn(SECRET, repr(S.ApiOptions(api_secret=SECRET)))
        self.assertNotIn(SECRET, repr(S.AccessToken(KEY, SECRET)))
        for value in (0, -1, 2**63):
            with self.assertRaises(ValueError):
                S.ApiOptions(timeout_ms=value)
        for kwargs in ({"timeout_ms": True}, {"url": None}, {"transport": False}):
            with self.subTest(kwargs=kwargs), self.assertRaises(TypeError):
                S.ApiOptions(**kwargs)
        with self.assertRaises(TypeError):
            S.LiveKitApi(S.ApiOptions(), url="other")
        with self.assertRaises(TypeError):
            S.LiveKitApi().room.create_room(M.Room())
        with self.assertRaises(ValueError):
            S.AccessToken(KEY, SECRET).to_jwt(now=datetime(2020, 1, 1))
        with self.assertRaises(ValueError):
            S.AccessToken(KEY, SECRET).set_valid_for(timedelta(microseconds=1))
        with self.assertRaises(TypeError):
            S.AccessToken(KEY, SECRET).set_valid_for(True)
        with self.assertRaises(TypeError):
            S.WebhookReceiver(api_key=KEY)
        with self.assertRaises(ValueError):
            S.WebhookReceiver(api_key=KEY, api_secret=SECRET, secrets={KEY: SECRET})
        self.assertEqual(S.Error("future", code=123).error_code, S.ErrorCode.UNKNOWN)

    def test_import_and_model_construction_do_not_load_native_code(self):
        environment = dict(os.environ, LIVEKIT_SERVER_PYTHON_LIBRARY="missing-test-library-does-not-exist")
        script = "import livekit_server as s; s.LiveKitApi(); s.models.Room(); s.VideoGrant(); print('lazy')"
        result = subprocess.run([sys.executable, "-c", script], env=environment,
                                capture_output=True, text=True, check=True)
        self.assertEqual(result.stdout.strip(), "lazy")

    def test_rejects_incompatible_abi_before_invocation(self):
        from unittest.mock import MagicMock
        library = MagicMock()
        library.lks_abi_version.return_value = 2
        with patch("livekit_server._native.C.CDLL", return_value=library):
            with self.assertRaisesRegex(ImportError, "ABI version"):
                S.NativeLibrary("unused-test-path")
        library.lks_invoke.assert_not_called()
        with patch("livekit_server._native.C.CDLL", return_value=object()):
            with self.assertRaisesRegex(ImportError, "C ABI exports"):
                S.NativeLibrary("unused-test-path")


class NativeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("LIVEKIT_SERVER_PYTHON_LIBRARY")
        if not path:
            raise unittest.SkipTest("set LIVEKIT_SERVER_PYTHON_LIBRARY for native ABI checks")
        cls.dll_dirs = tuple(filter(None, os.environ.get(
            "LIVEKIT_SERVER_PYTHON_DLL_DIRECTORIES", "").split(os.pathsep)))
        cls.native = S.NativeLibrary(path, dll_directories=cls.dll_dirs)

    def api(self, transport, **keywords):
        fields = {"url": "ws://offline.invalid/", "api_key": KEY, "api_secret": SECRET,
                  "timeout_ms": 1234, "transport": transport, **keywords}
        return S.LiveKitApi(native=self.native, **fields)

    def token(self):
        return S.AccessToken(KEY, SECRET, native=self.native)

    def test_default_loader_and_cached_library_in_subprocess(self):
        script = """
import os
from livekit_server import AccessToken
token = AccessToken("test-key", "test-only-secret-at-least-32-characters")
assert token.to_jwt(now=1700000000).count(".") == 2
os.environ["LIVEKIT_SERVER_PYTHON_LIBRARY"] = "missing-test-library-after-first-load"
assert token.to_jwt(now=1700000000).count(".") == 2
print("default ABI loader passed")
"""
        result = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, check=True)
        self.assertEqual(result.stdout.strip(), "default ABI loader passed")

    def test_all_52_service_routes_and_models(self):
        expected = ""
        count = 0
        thread = threading.get_ident()
        def transport(request):
            nonlocal count
            self.assertEqual(threading.get_ident(), thread)
            self.assertEqual(request.url, "http://offline.invalid/twirp/livekit." + expected)
            self.assertEqual(request.method, "POST")
            self.assertEqual(request.timeout_ms, 1234)
            self.assertIsInstance(request.body, bytes)
            self.assertEqual(dict(request.headers)["Content-Type"], "application/protobuf")
            self.assertTrue(dict(request.headers)["Authorization"].startswith("Bearer "))
            with self.assertRaises(FrozenInstanceError):
                request.body = b"no"
            count += 1
            return S.HttpResponse(200)
        api = self.api(transport)
        service_names = {"room": "RoomService", "egress": "Egress", "ingress": "Ingress", "sip": "SIP",
                         "agent_dispatch": "AgentDispatchService", "connector": "Connector"}
        for route in ROUTES:
            service, method, request, rpc = route.split()
            with self.subTest(route=route):
                expected = service_names[service] + "/" + rpc
                result = getattr(getattr(api, service), method)(getattr(M, request)())
                self.assertIsInstance(result, S.ProtocolModel)
                self.assertEqual(result.to_dict(), {})
        self.assertEqual(count, 52)
        self.assertEqual(len(ROUTES), 52)
        expected = "RoomService/ListRooms"
        self.assertEqual(api.room.list_rooms().to_dict(), {})
        expected = "AgentDispatchService/ListDispatch"
        self.assertIsNone(api.agent_dispatch.get_dispatch("missing", "room"))

    def test_binary_unicode_and_nul_round_trip(self):
        name = "room-\u4e2d\u6587-\U0001f600\x00"
        encoded = name.encode()
        def transport(request):
            self.assertEqual(request.body, bytes([10, len(encoded)]) + encoded)
            return S.HttpResponse(200, bytes([18, len(encoded)]) + encoded)
        response = self.api(transport).room.create_room(M.CreateRoomRequest(name=name))
        self.assertIsInstance(response, M.Room)
        self.assertEqual(response.to_dict()["name"], name)

    def test_preissued_token_and_environment_fallback(self):
        def transport(request):
            self.assertEqual(dict(request.headers)["Authorization"], "Bearer preissued-test-token")
            return S.HttpResponse(200)
        self.api(transport, api_key="", api_secret="", access_token="preissued-test-token").room.list_rooms()
        with patch.dict(os.environ, {"LIVEKIT_URL": "http://env.invalid", "LIVEKIT_API_KEY": KEY,
                                     "LIVEKIT_API_SECRET": SECRET, "LIVEKIT_TOKEN": ""}):
            def environment_transport(request):
                self.assertTrue(request.url.startswith("http://env.invalid/"))
                signature = dict(request.headers)["Authorization"].removeprefix("Bearer ").split(".")
                self.assertEqual(_unb64(signature[2]), hmac.digest(
                    SECRET.encode(), ".".join(signature[:2]).encode(), "sha256"))
                return S.HttpResponse(200)
            S.LiveKitApi(transport=environment_transport, native=self.native).room.list_rooms()

    def test_independent_hmac_grants_and_explicit_clock(self):
        attributes = {"\u8bed\u8a00": "\u4e2d\u6587\U0001f600"}
        token = (self.token().set_identity("alice\U0001f600").set_name("name")
                 .set_metadata('quote"\n\x00').set_attributes(attributes).set_valid_for(timedelta(seconds=60))
                 .set_video_grant(S.VideoGrant(room_join=True, room="room", can_publish=False,
                                               can_publish_sources=["microphone"]))
                 .set_sip_grant(S.SipGrant(call=True))
                 .set_agent_grant(S.AgentGrant(simulation_admin=True)))
        attributes["\u8bed\u8a00"] = "changed"
        jwt = token.to_jwt(now=datetime.fromtimestamp(1_700_000_000, timezone.utc))
        header, payload, signature = jwt.split(".")
        self.assertEqual(_unb64(signature), hmac.digest(SECRET.encode(), f"{header}.{payload}".encode(), "sha256"))
        claims = json.loads(_unb64(payload))
        self.assertEqual(claims["sub"], "alice\U0001f600")
        self.assertEqual(claims["metadata"], 'quote"\n\x00')
        self.assertEqual(claims["attributes"]["\u8bed\u8a00"], "\u4e2d\u6587\U0001f600")
        self.assertFalse(claims["video"]["canPublish"])
        self.assertNotIn("canSubscribe", claims["video"])
        self.assertEqual(claims["video"]["canPublishSources"], ["microphone"])
        self.assertTrue(claims["sip"]["call"])
        self.assertTrue(claims["agent"]["simulationAdmin"])
        self.assertEqual((claims["nbf"], claims["exp"]), (1_700_000_000, 1_700_000_060))

    def test_sdk_errors_keep_categories_status_and_twirp_details(self):
        def failure(code, action):
            with self.assertRaises(S.Error) as raised:
                action()
            self.assertEqual(raised.exception.error_code, code)
            return raised.exception
        failure(S.ErrorCode.AUTHENTICATION, lambda: S.AccessToken("", "", native=self.native).to_jwt())
        failure(S.ErrorCode.INVALID_ARGUMENT, lambda: self.token().to_jwt(now=-1))
        failure(S.ErrorCode.INVALID_ARGUMENT, lambda: self.token().to_jwt(
            now=datetime(1969, 12, 31, 23, 59, 59, 500000, timezone.utc)))
        failure(S.ErrorCode.INVALID_ARGUMENT, lambda: self.token().set_valid_for(2**63 - 1).to_jwt())
        api = self.api(lambda _: S.HttpResponse(200))
        failure(S.ErrorCode.PROTOCOL, lambda: api.room.create_room(M.CreateRoomRequest("not JSON")))
        failure(S.ErrorCode.PROTOCOL, lambda: api.room.create_room(M.CreateRoomRequest(unknown=True)))
        failure(S.ErrorCode.PROTOCOL, lambda: self.api(lambda _: S.HttpResponse(200, b"\xff")).room.list_rooms())
        failure(S.ErrorCode.TRANSPORT, lambda: self.api(lambda _: S.HttpResponse(999)).room.list_rooms())
        message = "denied \u4e2d\u6587\U0001f600"
        error = failure(S.ErrorCode.HTTP, lambda: self.api(lambda _: S.HttpResponse(
            403, json.dumps({"code": "permission_denied", "msg": message}).encode())).room.list_rooms())
        self.assertEqual((error.http_status, error.twirp_code, str(error)), (403, "permission_denied", message))

    def test_host_callback_exceptions_and_result_freeing(self):
        native = S.NativeLibrary(os.environ["LIVEKIT_SERVER_PYTHON_LIBRARY"], dll_directories=self.dll_dirs)
        original_free = native._library.lks_free
        released = []
        def release(pointer):
            released.append(pointer)
            original_free(pointer)
        sentinel = KeyboardInterrupt("test callback interruption")
        def interrupted(_):
            raise sentinel
        with patch.object(native._library, "lks_free", release):
            with self.assertRaises(KeyboardInterrupt) as raised:
                S.LiveKitApi(native=native, transport=interrupted, access_token="test",
                             url="http://offline.invalid").room.list_rooms()
            self.assertIs(raised.exception, sentinel)
            self.assertEqual(len(released), 1)
            with self.assertRaises(S.Error):
                native.invoke({"operation": "invalid"})
            self.assertEqual(len(released), 2)
            S.AccessToken(KEY, SECRET, native=native).to_jwt(now=1_700_000_000)
            self.assertEqual(len(released), 3)
            with patch.object(native._library, "lks_invoke", return_value=None):
                with self.assertRaises(MemoryError):
                    native.invoke({"operation": "token"})
            self.assertEqual(len(released), 3)
        for result in (None, {}, S.HttpResponse(True), S.HttpResponse(200, "not bytes")):
            with self.subTest(result=result), self.assertRaises(TypeError):
                self.api(lambda _: result).room.list_rooms()

    def test_nested_calls_gc_and_concurrent_callbacks(self):
        def transport(request):
            current = threading.get_ident()
            self.token().to_jwt(now=1_700_000_000)
            gc.collect()
            self.assertEqual(threading.get_ident(), current)
            name = "\u4e2d\u6587\U0001f600".encode()
            return S.HttpResponse(200, bytes([18, len(name)]) + name)
        api = self.api(transport)
        with ThreadPoolExecutor(max_workers=6) as executor:
            responses = list(executor.map(lambda _: api.room.create_room(M.CreateRoomRequest()), range(24)))
        self.assertTrue(all(result.to_dict()["name"] == "\u4e2d\u6587\U0001f600" for result in responses))

    def test_dispatch_lookup_found(self):
        # ListAgentDispatchResponse.agent_dispatches[0] = AgentDispatch{id: "AD_1"}.
        response = b"\x0a\x06\x0a\x04AD_1"
        api = self.api(lambda _: S.HttpResponse(200, response))
        self.assertEqual(api.agent_dispatch.get_dispatch("AD_1", "room").to_dict()["id"], "AD_1")
        # Filtering is performed by the server; an empty response means no dispatch exists.
        missing = self.api(lambda _: S.HttpResponse(200))
        self.assertIsNone(missing.agent_dispatch.get_dispatch("other", "room"))

    def test_verified_webhook_snapshots_and_raw_payload(self):
        body = json.dumps({"event": "participant_joined", "id": "EV_1", "createdAt": "1700000000",
                           "room": {"sid": "RM_1", "name": "room", "metadata": "r"},
                           "participant": {"sid": "PA_1", "identity": "alice", "name": "A", "metadata": "p"},
                           "track": {"sid": "TR_1", "name": "microphone"},
                           "egressInfo": {"egressId": "EG_1", "roomId": "RM_1", "roomName": "room"},
                           "ingressInfo": {"ingressId": "IN_1", "name": "input", "roomName": "room"},
                           "futureField": {"\u4e2d\u6587": True}}, ensure_ascii=False).encode()
        seen = []
        receiver = S.WebhookReceiver(api_key=KEY, api_secret=SECRET, native=self.native, callback=seen.append)
        event = receiver.receive_and_dispatch(body, "Bearer " + authorization(body))
        self.assertEqual(seen, [event])
        self.assertEqual(event.raw_body.encode(), body)
        self.assertEqual(event.created_at, 1_700_000_000)
        self.assertEqual(event.room.sid, "RM_1")
        self.assertEqual(event.participant.identity, "alice")
        self.assertEqual(event.track.sid, "TR_1")
        self.assertEqual(event.egress.egress_id, "EG_1")
        self.assertEqual(event.ingress.ingress_id, "IN_1")
        with self.assertRaises(FrozenInstanceError):
            event.room.name = "changed"

    def test_webhook_tampering_lifetime_and_key_rotation(self):
        body = b'{"event":"room_started","id":"EV_2"}'
        token = authorization(body)
        called = []
        receiver = S.WebhookReceiver(secrets={KEY: SECRET}, callback=called.append, native=self.native)
        prefix, signature = token.rsplit(".", 1)
        bad_signature = ("A" if signature[0] != "A" else "B") + signature[1:]
        now = int(time.time())
        for payload, jwt in ((body + b" ", token), (body, prefix + "." + bad_signature),
                             (body, authorization(body, iss="unknown")),
                             (body, authorization(body, nbf=now - 7200, exp=now - 3600)),
                             (body, authorization(body, nbf=now + 3600, exp=now + 7200))):
            with self.subTest(jwt=jwt[:10]), self.assertRaises(S.Error) as raised:
                receiver.receive_and_dispatch(payload, jwt)
            self.assertEqual(raised.exception.error_code, S.ErrorCode.AUTHENTICATION)
        self.assertEqual(called, [])
        keys = []
        def key_provider(key):
            keys.append(key)
            return SECRET if key == KEY else None
        dynamic = S.WebhookReceiver(key_provider=key_provider, native=self.native)
        self.assertEqual(dynamic.receive(body, token).id, "EV_2")
        self.assertEqual(keys, [KEY])
        with self.assertRaises(S.Error):
            dynamic.receive(body, authorization(body, iss="unknown"))
        sentinel = ValueError("key provider failed")
        def failed(_):
            raise sentinel
        with self.assertRaises(ValueError) as raised:
            S.WebhookReceiver(key_provider=failed, native=self.native).receive(body, token)
        self.assertIs(raised.exception, sentinel)
        with self.assertRaises(TypeError):
            S.WebhookReceiver(key_provider=lambda _: 123, native=self.native).receive(body, token)

    def test_webhook_callback_replacement_is_reentrant_and_exception_safe(self):
        body = b'{"event":"room_started","id":"EV_3"}'
        receiver = S.WebhookReceiver(api_key=KEY, api_secret=SECRET, native=self.native)
        event = receiver.receive(body, authorization(body))
        seen = []
        def replace(value):
            receiver.set_callback(seen.append)
            receiver.dispatch(value)  # Must run outside the callback mutex.
        receiver.set_callback(replace).dispatch(event)
        self.assertEqual(seen, [event])
        receiver.set_callback(None).dispatch(event)
        self.assertEqual(seen, [event])
        sentinel = RuntimeError("event callback failed")
        def failed(_):
            raise sentinel
        with self.assertRaises(RuntimeError) as raised:
            receiver.set_callback(failed).dispatch(event)
        self.assertIs(raised.exception, sentinel)


if __name__ == "__main__":
    unittest.main(verbosity=2)
