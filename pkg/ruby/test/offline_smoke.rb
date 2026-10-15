# frozen_string_literal: true
require 'livekit/server'
require 'openssl'
require 'digest'

module OfflineSmoke
  S = LiveKit::Server
  M = S::Models
  KEY = 'ruby-test-key'
  SECRET = 'ruby-test-secret-at-least-32-characters'
  @checks = 0

  def self.check(condition, message)
    @checks += 1
    raise "check failed: #{message}" unless condition
  end

  def self.failure(code)
    yield
    raise "expected #{code}"
  rescue S::Error => error
    check(error.error_code == code, "error code #{error.error_code}, expected #{code}")
    error
  end

  def self.api(&transport)
    S::LiveKitApi.new(url: 'ws://offline.invalid/', api_key: KEY, api_secret: SECRET,
                     timeout_ms: 1234, transport: transport)
  end

  def self.b64(value) = Base64.urlsafe_encode64(value, padding: false)
  def self.mac(value) = OpenSSL::HMAC.digest('SHA256', SECRET, value)
  def self.sign(claims)
    input = "#{b64(JSON.generate(alg: 'HS256', typ: 'JWT'))}.#{b64(JSON.generate(claims))}"
    "#{input}.#{b64(mac(input))}"
  end

  def self.tokens
    sources = ['microphone']
    grant = S::VideoGrant.new(room_join: true, room: '房间😀', can_publish: false,
      can_subscribe: nil, can_publish_data: false, can_update_own_metadata: true,
      can_publish_sources: sources, destination_room: 'next', hidden: true)
    sources << 'camera'
    token = S::AccessToken.new(api_key: KEY, api_secret: SECRET).set_identity('用户😀')
      .set_name('name').set_metadata("quote\"\n\0").set_attributes('语言' => '中文😀')
      .set_valid_for(60).set_video_grant(grant).set_sip_grant(S::SipGrant.new(call: true))
      .set_agent_grant(S::AgentGrant.new(simulation_admin: true)).to_jwt(now: 1_700_000_000)
    header, payload, signature = token.split('.')
    check(Base64.urlsafe_decode64(signature) == mac("#{header}.#{payload}"), 'independent JWT HMAC')
    claims = JSON.parse(Base64.urlsafe_decode64(payload))
    check(claims['sub'] == '用户😀' && claims['video']['room'] == '房间😀', 'JWT UTF-8')
    check(claims['video']['canPublish'] == false, 'explicit false grant')
    check(!claims['video'].key?('canSubscribe'), 'absent optional grant')
    check(claims['video']['canPublishSources'] == ['microphone'], 'grant snapshot')
    check(claims['metadata'] == "quote\"\n\0" && claims['attributes']['语言'] == '中文😀', 'metadata and attributes')
    check(claims['exp'] == 1_700_000_060 && claims['nbf'] == 1_700_000_000, 'explicit token clock')
    check(claims['sip']['call'] && claims['agent']['simulationAdmin'], 'SIP and Agent grants')
    failure(:authentication) { S::AccessToken.new(api_key: '', api_secret: '').to_jwt }
    failure(:invalid_argument) { S::AccessToken.new(api_key: KEY, api_secret: SECRET).set_valid_for(2**63 - 1).to_jwt }
    failure(:invalid_argument) { S::AccessToken.new(api_key: KEY, api_secret: SECRET).to_jwt(now: -1) }
    check(!S::ApiOptions.new(api_secret: SECRET).inspect.include?(SECRET), 'credentials redacted')
  end

  ROUTES = <<~ROUTES.freeze
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
  ROUTES

  def self.services
    expected = nil
    calls = 0
    client = api do |request|
      check(request.url == "http://offline.invalid/twirp/livekit.#{expected}", "RPC route #{request.url}")
      check(request.method == 'POST' && request.timeout_ms == 1234, 'HTTP options')
      headers = request.headers.to_h
      check(headers['Content-Type'] == 'application/protobuf', 'binary content type')
      check(headers['Authorization'].start_with?('Bearer '), 'request authorization')
      check(request.body.encoding == Encoding::BINARY, 'binary body encoding')
      calls += 1
      S::HttpResponse.new(status_code: 200, body: ''.b)
    end
    service_names = { 'room' => 'RoomService', 'egress' => 'Egress', 'ingress' => 'Ingress',
                      'sip' => 'SIP', 'agent_dispatch' => 'AgentDispatchService', 'connector' => 'Connector' }
    ROUTES.each_line do |line|
      service, method, request, rpc = line.split
      expected = "#{service_names.fetch(service)}/#{rpc}"
      result = client.public_send(service).public_send(method, M.const_get(request).new)
      check(result.is_a?(S::ProtocolModel), 'typed response')
    end
    check(calls == 52, 'all 52 service methods')
    check(M.constants(false).length == 86, 'all 86 models')
    expected = 'RoomService/ListRooms'
    check(client.room.list_rooms.to_h == {}, 'default list_rooms request')
    expected = 'AgentDispatchService/ListDispatch'
    check(client.agent_dispatch.get_dispatch('missing', 'room').nil?, 'missing dispatch')
    name = '房间😀'
    echo = api do |request|
      check(request.body == [10, name.bytesize].pack('C*') + name.b, 'UTF-8 binary request')
      S::HttpResponse.new(status_code: 200, body: [18, name.bytesize].pack('C*') + name.b)
    end
    check(echo.room.create_room(M::CreateRoomRequest.new(name: name)).to_h['name'] == name, 'UTF-8 response')
    preissued = S::LiveKitApi.new(url: 'https://offline.invalid/', access_token: 'preissued-test-token',
      transport: lambda { |request|
        check(request.headers.to_h['Authorization'] == 'Bearer preissued-test-token', 'pre-issued token')
        S::HttpResponse.new(status_code: 200, body: '')
      })
    preissued.room.list_rooms
    http = failure(:http) do
      api { |_req| S::HttpResponse.new(status_code: 403, body: JSON.generate(code: 'permission_denied', msg: '拒绝😀')) }.room.list_rooms
    end
    check(http.http_status == 403 && http.twirp_code == 'permission_denied' && http.message == '拒绝😀', 'error details')
    failure(:protocol) { client.room.create_room(M::CreateRoomRequest.new('not JSON')) }
    failure(:protocol) { api { |_req| S::HttpResponse.new(status_code: 200, body: "\xff".b) }.room.list_rooms }
    failure(:transport) { api { |_req| S::HttpResponse.new(status_code: 0, body: '') }.room.list_rooms }
    sentinel = IOError.new('transport failure')
    begin
      api { |_req| raise sentinel }.room.list_rooms
      raise 'missing Ruby callback exception'
    rescue IOError => error
      check(error.equal?(sentinel), 'callback exception preserved')
    end
    gc_client = api do |_request|
      GC.start
      GC.compact if GC.respond_to?(:compact)
      S::HttpResponse.new(status_code: 200, body: '')
    end
    5.times { gc_client.room.list_rooms }
    check(true, 'callback memory survives GC and compaction')
    threaded = api { |_request| S::HttpResponse.new(status_code: 200, body: '') }
    8.times.map { Thread.new { 4.times { threaded.room.list_rooms } } }.each(&:value)
    check(true, 'concurrent native calls')
    begin
      client.room.create_room(M::DeleteRoomRequest.new)
      raise 'wrong model type accepted'
    rescue ArgumentError
      check(true, 'request type checked')
    end
  end

  def self.webhooks
    body = JSON.generate(event: 'room_started', id: 'event-😀', createdAt: '1700000000',
      room: { name: '房间😀' }, participant: { identity: '用户😀' }, track: { sid: 'TR_test' },
      egressInfo: { egressId: 'EG_test' }, ingressInfo: { ingressId: 'IN_test' }, futureField: true)
    auth = sign(iss: KEY, sha256: Base64.strict_encode64(Digest::SHA256.digest(body)))
    delivered = []
    receiver = S::WebhookReceiver.new(secrets: { 'old' => 'old-secret', KEY => SECRET }) { |event| delivered << event }
    event = receiver.receive_and_dispatch(body.b, "Bearer #{auth}")
    check(event.event == 'room_started' && event.id == 'event-😀' && event.created_at == 1_700_000_000, 'event fields')
    check(event.raw_body == body && event.room.name == '房间😀', 'verified raw payload and room')
    check(event.frozen? && event.raw_body.frozen? && event.room.name.frozen?, 'immutable webhook snapshot')
    check(event.participant.identity == '用户😀' && event.track.sid == 'TR_test', 'participant and track')
    check(event.egress.egress_id == 'EG_test' && event.ingress.ingress_id == 'IN_test', 'egress and ingress')
    check(delivered == [event], 'callback delivered')
    failure(:authentication) { receiver.receive_and_dispatch(body + ' ', auth) }
    check(delivered == [event], 'tampered event never dispatched')
    failure(:authentication) { S::WebhookReceiver.new(api_key: KEY, api_secret: 'wrong').receive(body, auth) }
    failure(:authentication) { receiver.receive(body, '') }
    provider = S::WebhookReceiver.new(key_provider: ->(key) { GC.start; key == KEY ? SECRET : nil })
    check(provider.receive(body, auth) == event, 'dynamic key provider')
    failure(:authentication) { S::WebhookReceiver.new(key_provider: ->(_key) { nil }).receive(body, auth) }
    sentinel = IOError.new('key lookup failure')
    begin
      S::WebhookReceiver.new(key_provider: ->(_key) { raise sentinel }).receive(body, auth)
      raise 'missing key provider exception'
    rescue IOError => error
      check(error.equal?(sentinel), 'key exception preserved')
    end
    receiver.set_callback { |_value| raise sentinel }
    begin
      receiver.receive_and_dispatch(body, auth)
      raise 'missing event callback exception'
    rescue IOError => error
      check(error.equal?(sentinel), 'event callback exception preserved')
    end
    expired = sign(iss: KEY, exp: 1, sha256: Base64.strict_encode64(Digest::SHA256.digest(body)))
    failure(:authentication) { receiver.receive(body, expired) }
  end

  tokens
  services
  webhooks
  puts "Offline Ruby smoke passed: #{@checks} checks"
end
