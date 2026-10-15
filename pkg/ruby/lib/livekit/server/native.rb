# frozen_string_literal: true
require 'fiddle'
require 'rbconfig'

module LiveKit
  module Server
    module Native
      @load_lock = Mutex.new

      def self.load_bridge
        @load_lock.synchronize do
          return if @invoke
          configured = ENV['LIVEKIT_SERVER_RUBY_LIBRARY']
          default = if RbConfig::CONFIG['host_os'].match?(/mswin|mingw/)
                      'livekitserver_ruby.dll'
                    elsif RbConfig::CONFIG['host_os'].include?('darwin')
                      'liblivekitserver_ruby.dylib'
                    else
                      'liblivekitserver_ruby.so'
                    end
          library = configured && !configured.empty? ? File.expand_path(configured) : default
          @handle = Fiddle.dlopen(library)
          abi = Fiddle::Function.new(@handle['lks_ruby_abi_version'], [], Fiddle::TYPE_INT)
          raise LoadError, 'unsupported LiveKit Ruby bridge ABI' unless abi.call == 1
          @free = Fiddle::Function.new(@handle['lks_ruby_free'], [Fiddle::TYPE_VOIDP], Fiddle::TYPE_VOID)
          @invoke = Fiddle::Function.new(@handle['lks_ruby_invoke'],
            [Fiddle::TYPE_VOIDP, Fiddle::TYPE_SIZE_T, Fiddle::TYPE_VOIDP], Fiddle::TYPE_VOIDP)
        end
      end

      def self.invoke(input = nil, transport: nil, key_provider: nil, **keywords)
        load_bridge
        input ||= keywords
        # Keep every callback result buffer and closure alive until native code has returned.
        buffers = []
        callback_error = nil
        fallback_text = "{\"error\":true}\0"
        fallback = Fiddle::Pointer.malloc(fallback_text.bytesize, Fiddle::RUBY_FREE)
        fallback[0, fallback_text.bytesize] = fallback_text
        callback = Fiddle::Closure::BlockCaller.new(Fiddle::TYPE_VOIDP,
          [Fiddle::TYPE_VOIDP, Fiddle::TYPE_SIZE_T]) do |pointer, length|
          begin
            request = JSON.parse(Fiddle::Pointer.new(pointer)[0, length].force_encoding(Encoding::UTF_8))
            response = case request.fetch('kind')
                       when 'transport'
                         body = Base64.urlsafe_decode64(request.fetch('body')).b.freeze
                         headers = request.fetch('headers').map { |pair| pair.map(&:freeze).freeze }.freeze
                         http = HttpRequest.new(url: request.fetch('url').freeze, method: request.fetch('method').freeze,
                           headers: headers, body: body, timeout_ms: Integer(request.fetch('timeout_ms'))).freeze
                         result = transport.call(http)
                         raise TypeError, 'transport must return HttpResponse' unless result.is_a?(HttpResponse)
                         raise TypeError, 'response body must be a String' unless result.body.is_a?(String)
                         raise TypeError, 'response status must be an Integer' unless result.status_code.is_a?(Integer)
                         { status_code: result.status_code.to_s, body: Base64.urlsafe_encode64(result.body, padding: false) }
                       when 'key'
                         secret = key_provider.call(request.fetch('key'))
                         raise TypeError, 'key provider must return a String or nil' unless secret.nil? || secret.is_a?(String)
                         { secret: secret || '' }
                       else
                         raise Error.new('unknown native callback', code: 5)
                       end
            buffer = JSON.generate(response) + "\0"
            allocation = Fiddle::Pointer.malloc(buffer.bytesize, Fiddle::RUBY_FREE)
            allocation[0, buffer.bytesize] = buffer
            buffers << allocation
            allocation.to_i
          rescue Exception => error # Never unwind Ruby exceptions through a C++/Fiddle stack.
            callback_error ||= error
            fallback.to_i
          end
        end
        json = JSON.generate(input)
        output = nil
        begin
          output = @invoke.call(json, json.bytesize, callback)
          raise callback_error if callback_error
          raise NoMemoryError, 'native Ruby bridge could not allocate its result' if output.null?
          response = JSON.parse(output.to_s.force_encoding(Encoding::UTF_8))
          unless response.fetch('ok')
            raise Error.new(response.fetch('message'), code: response.fetch('code').to_i,
              http_status: response.fetch('http_status').to_i, twirp_code: response.fetch('twirp_code'))
          end
          response['value']
        ensure
          @free.call(output) if output && !output.null?
          # Explicitly retain these references across Fiddle calls and callbacks, including GC.
          callback = nil
          buffers.each(&:call_free)
          buffers.clear
          fallback.call_free
        end
      end
    end
  end
end
