# frozen_string_literal: true
require 'json'
require 'base64'
require_relative 'server/version'
require_relative 'server/native'

module LiveKit
  module Server
    class Error < StandardError
      CODES = { 0 => :invalid_argument, 1 => :authentication, 2 => :transport,
                3 => :http, 4 => :protocol, 5 => :unsupported, 255 => :unknown }.freeze
      attr_reader :code, :http_status, :twirp_code

      def initialize(message, code:, http_status: 0, twirp_code: '')
        super(message)
        @code, @http_status, @twirp_code = code, http_status, twirp_code
      end

      def error_code = CODES.fetch(code, :unknown)
    end

    class ProtocolModel
      attr_reader :json

      def initialize(value = {})
        raise ArgumentError, 'expected a Hash or JSON String' unless value.is_a?(Hash) || value.is_a?(String)
        @json = (value.is_a?(String) ? value.encode(Encoding::UTF_8) : JSON.generate(value)).dup.freeze
        freeze
      end

      def self.from_json(json) = new(json)
      def to_h = JSON.parse(json)
      def empty? = json.empty? || json == '{}'
      def ==(other) = other.instance_of?(self.class) && json == other.json
      alias eql? ==
      def hash = [self.class, json].hash
      def to_s = json
    end

    # Grant values are copied and validated at construction. Nil optional booleans are omitted.
    class Grant
      def initialize(**values)
        unknown = values.keys - self.class::FIELDS.keys
        raise ArgumentError, "unknown grant fields: #{unknown.join(', ')}" unless unknown.empty?
        @fields = values.each_with_object({}) do |(key, value), result|
          type = self.class::FIELDS.fetch(key)
          next if value.nil? && type == :optional_boolean
          valid = case type
                  when :boolean, :optional_boolean then value == true || value == false
                  when :string then value.is_a?(String)
                  when :strings then value.is_a?(Array) && value.all? { |item| item.is_a?(String) }
                  end
          raise ArgumentError, "invalid grant field #{key}" unless valid
          result[key] = value.is_a?(Array) ? value.map { |item| item.dup.freeze }.freeze : value.is_a?(String) ? value.dup.freeze : value
        end.freeze
        freeze
      end

      def to_h = @fields.dup
    end

    class AccessToken
      def initialize(api_key:, api_secret:)
        unless api_key.is_a?(String) && api_secret.is_a?(String)
          raise ArgumentError, 'API key and secret must be Strings'
        end
        @fields = { api_key: api_key.dup, api_secret: api_secret.dup, valid_for: '21600' }
      end

      %i[identity name metadata].each do |field|
        define_method("set_#{field}") do |value|
          raise ArgumentError, "#{field} must be a String" unless value.is_a?(String)
          @fields[field] = value.dup
          self
        end
      end

      def set_attributes(attributes)
        unless attributes.is_a?(Hash) && attributes.all? { |key, value| key.is_a?(String) && value.is_a?(String) }
          raise ArgumentError, 'attributes must be a String-to-String Hash'
        end
        @fields[:attributes] = attributes.to_h { |key, value| [key.dup, value.dup] }
        self
      end

      def set_valid_for(seconds)
        raise ArgumentError, 'valid_for must be a positive Integer in seconds' unless seconds.is_a?(Integer) && seconds.positive?
        @fields[:valid_for] = seconds.to_s
        self
      end

      %i[video sip agent].each do |kind|
        define_method("set_#{kind}_grant") do |grant|
          expected = Server.const_get("#{kind.capitalize}Grant")
          raise ArgumentError, "expected #{expected}" unless grant.is_a?(expected)
          @fields[kind] = grant.to_h
          self
        end
      end

      def to_jwt(now: Time.now)
        seconds = now.is_a?(Time) ? now.to_i : now
        raise ArgumentError, 'now must be a Time or Integer epoch seconds' unless seconds.is_a?(Integer)
        Native.invoke(operation: 'token', options: @fields.merge(now: seconds.to_s))
      end

      def inspect = '#<LiveKit::Server::AccessToken credentials redacted>'
    end

    # Binary protobuf transport snapshots. Callbacks receive HttpRequest and return HttpResponse.
    HttpRequest = Struct.new(:url, :method, :headers, :body, :timeout_ms, keyword_init: true)
    HttpResponse = Struct.new(:status_code, :body, keyword_init: true)

    class ApiOptions
      attr_reader :transport

      def initialize(url: '', api_key: '', api_secret: '', access_token: '', timeout_ms: 30_000, transport: nil)
        unless [url, api_key, api_secret, access_token].all? { |value| value.is_a?(String) }
          raise ArgumentError, 'URL and credentials must be Strings'
        end
        raise ArgumentError, 'timeout_ms must be a positive Integer' unless timeout_ms.is_a?(Integer) && timeout_ms.positive? && timeout_ms <= 2**63 - 1
        raise ArgumentError, 'transport must respond to call' if transport && !transport.respond_to?(:call)
        @fields = { url: url.dup.freeze, api_key: api_key.dup.freeze, api_secret: api_secret.dup.freeze,
                    access_token: access_token.dup.freeze, timeout_ms: timeout_ms.to_s,
                    use_transport: !transport.nil? }.freeze
        @transport = transport
        freeze
      end

      def to_h = @fields.dup
      def inspect = '#<LiveKit::Server::ApiOptions credentials redacted>'
    end

    class LiveKitApi
      attr_reader :room, :egress, :ingress, :sip, :agent_dispatch, :connector

      def initialize(options = nil, **keywords)
        raise ArgumentError, 'pass options or keywords, not both' if options && !keywords.empty?
        @options = options || ApiOptions.new(**keywords)
        raise ArgumentError, 'expected ApiOptions' unless @options.is_a?(ApiOptions)
        @room = RoomServiceClient.new(self)
        @egress = EgressClient.new(self)
        @ingress = IngressClient.new(self)
        @sip = SipClient.new(self)
        @agent_dispatch = AgentDispatchClient.new(self)
        @connector = ConnectorClient.new(self)
        freeze
      end

      def call(operation, request)
        Native.invoke({ operation: operation, options: @options.to_h, request: request }, transport: @options.transport)
      end

      def get_dispatch(id, room)
        Native.invoke({ operation: 'get_dispatch', options: @options.to_h, dispatch_id: id, room: room },
                      transport: @options.transport)
      end
    end

    WebhookRoom = Struct.new(:sid, :name, :metadata, keyword_init: true)
    WebhookParticipant = Struct.new(:sid, :identity, :name, :metadata, keyword_init: true)
    WebhookTrack = Struct.new(:sid, :name, keyword_init: true)
    WebhookEgress = Struct.new(:egress_id, :room_id, :room_name, keyword_init: true)
    WebhookIngress = Struct.new(:ingress_id, :name, :room_name, keyword_init: true)
    WebhookEvent = Struct.new(:event, :id, :created_at, :room, :participant, :track, :egress, :ingress, :raw_body,
                             keyword_init: true)

    class WebhookReceiver
      def initialize(api_key: nil, api_secret: nil, secrets: nil, key_provider: nil, &callback)
        modes = [!api_key.nil? || !api_secret.nil?, !secrets.nil?, !key_provider.nil?].count(true)
        raise ArgumentError, 'provide key/secret, secrets Hash, or key_provider' unless modes == 1
        if key_provider && !key_provider.respond_to?(:call)
          raise ArgumentError, 'key_provider must respond to call'
        end
        @secrets = secrets || (key_provider ? {} : { api_key => api_secret })
        unless @secrets.is_a?(Hash) && @secrets.all? { |key, secret| key.is_a?(String) && secret.is_a?(String) }
          raise ArgumentError, 'secrets must be a String-to-String Hash'
        end
        @secrets = @secrets.to_h { |key, value| [key.dup.freeze, value.dup.freeze] }.freeze
        @key_provider = key_provider
        @callback_lock = Mutex.new
        @callback = callback
      end

      def set_callback(callback = nil, &block)
        selected = callback || block
        raise ArgumentError, 'callback must respond to call' unless selected.respond_to?(:call)
        @callback_lock.synchronize { @callback = selected }
        self
      end

      def receive(body, authorization)
        raise ArgumentError, 'body and authorization must be Strings' unless body.is_a?(String) && authorization.is_a?(String)
        result = Native.invoke({ operation: 'webhook', options: { body: Base64.urlsafe_encode64(body, padding: false),
          authorization: authorization, secrets: @secrets, use_key_provider: !@key_provider.nil? } },
          key_provider: @key_provider)
        values = result.transform_keys(&:to_sym)
        values.each_value do |value|
          value.each_value(&:freeze) if value.is_a?(Hash)
          value.freeze
        end
        values[:created_at] = Integer(values[:created_at])
        { room: WebhookRoom, participant: WebhookParticipant, track: WebhookTrack,
          egress: WebhookEgress, ingress: WebhookIngress }.each do |key, type|
          values[key] = type.new(**values[key].transform_keys(&:to_sym)).freeze if values[key]
        end
        WebhookEvent.new(**values).freeze
      end

      def dispatch(event)
        raise ArgumentError, 'expected WebhookEvent' unless event.is_a?(WebhookEvent)
        callback = @callback_lock.synchronize { @callback }
        callback&.call(event)
        nil
      end

      def receive_and_dispatch(body, authorization)
        event = receive(body, authorization)
        dispatch(event)
        event
      end
    end
  end
end

require_relative 'server/grants'
require_relative 'server/generated'
