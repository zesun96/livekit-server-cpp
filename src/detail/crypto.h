#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace livekit::server::detail {

[[nodiscard]] std::vector<std::uint8_t> Sha256(std::string_view value);
[[nodiscard]] std::vector<std::uint8_t> HmacSha256(std::string_view secret, std::string_view value);
[[nodiscard]] std::string Base64Encode(std::span<const std::uint8_t> value);
[[nodiscard]] std::string Base64UrlEncode(std::span<const std::uint8_t> value);
[[nodiscard]] std::string Base64UrlEncode(std::string_view value);
[[nodiscard]] std::vector<std::uint8_t> Base64UrlDecode(std::string_view value);
[[nodiscard]] bool ConstantTimeEqual(std::span<const std::uint8_t> left,
                                     std::span<const std::uint8_t> right) noexcept;
[[nodiscard]] bool ConstantTimeEqual(std::string_view left, std::string_view right) noexcept;

} // namespace livekit::server::detail
