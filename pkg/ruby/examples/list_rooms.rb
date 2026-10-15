# frozen_string_literal: true
require 'livekit/server'

# Reads LIVEKIT_URL and LIVEKIT_TOKEN or LIVEKIT_API_KEY/LIVEKIT_API_SECRET.
api = LiveKit::Server::LiveKitApi.new
puts api.room.list_rooms.json
