# frozen_string_literal: true
require_relative 'lib/livekit/server/version'

Gem::Specification.new do |spec|
  spec.name = 'livekit-server-cpp'
  spec.version = LiveKit::Server::VERSION
  spec.summary = 'Ruby binding for the LiveKit C++ server SDK'
  spec.authors = ['LiveKit C++ SDK contributors']
  spec.license = 'Apache-2.0'
  spec.required_ruby_version = '>= 3.1'
  spec.files = Dir['lib/**/*.rb', 'scripts/*.py', 'src/*', 'examples/*.rb', 'test/*.rb', 'README.md', 'LICENSE', 'CMakeLists.txt']
  spec.require_paths = ['lib']
  spec.add_dependency 'fiddle', '>= 1.1', '< 2'
  spec.add_dependency 'json', '>= 2.6', '< 3'
  spec.add_dependency 'base64', '>= 0.1', '< 1'
end
