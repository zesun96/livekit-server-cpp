#include "detail/crypto.h"

#include "livekit/server/error.h"

#ifdef _WIN32
#include <windows.h>

#include <bcrypt.h>
#endif

#include <limits>

namespace livekit::server::detail {
namespace {

std::string Base64(std::span<const std::uint8_t> data, const char* alphabet, bool padding) {
	std::string output;
	output.reserve(((data.size() + 2) / 3) * 4);
	for (std::size_t offset = 0; offset < data.size(); offset += 3) {
		const std::uint32_t first = data[offset];
		const std::uint32_t second = offset + 1 < data.size() ? data[offset + 1] : 0;
		const std::uint32_t third = offset + 2 < data.size() ? data[offset + 2] : 0;
		const std::uint32_t value = (first << 16) | (second << 8) | third;
		output.push_back(alphabet[(value >> 18) & 0x3f]);
		output.push_back(alphabet[(value >> 12) & 0x3f]);
		if (offset + 1 < data.size()) {
			output.push_back(alphabet[(value >> 6) & 0x3f]);
		} else if (padding) {
			output.push_back('=');
		}
		if (offset + 2 < data.size()) {
			output.push_back(alphabet[value & 0x3f]);
		} else if (padding) {
			output.push_back('=');
		}
	}
	return output;
}

#ifdef _WIN32
std::vector<std::uint8_t> BCryptDigest(std::string_view secret, std::string_view value, bool hmac) {
	if (secret.size() > std::numeric_limits<ULONG>::max() ||
	    value.size() > std::numeric_limits<ULONG>::max()) {
		throw Error(ErrorCode::invalid_argument, "cryptographic input is too large");
	}
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	DWORD object_size = 0;
	DWORD hash_size = 0;
	DWORD copied = 0;
	const ULONG flags = hmac ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0;
	if (!BCRYPT_SUCCESS(
	        BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, flags)) ||
	    !BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
	                                      reinterpret_cast<PUCHAR>(&object_size),
	                                      sizeof(object_size), &copied, 0)) ||
	    !BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
	                                      reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size),
	                                      &copied, 0))) {
		if (algorithm != nullptr) {
			BCryptCloseAlgorithmProvider(algorithm, 0);
		}
		throw Error(ErrorCode::authentication, "failed to initialize SHA-256");
	}

	std::vector<std::uint8_t> object(object_size);
	std::vector<std::uint8_t> digest(hash_size);
	PUCHAR secret_data =
	    hmac ? reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())) : nullptr;
	const ULONG secret_size = hmac ? static_cast<ULONG>(secret.size()) : 0;
	const auto create_status =
	    BCryptCreateHash(algorithm, &hash, object.data(), static_cast<ULONG>(object.size()),
	                     secret_data, secret_size, 0);
	const auto hash_status =
	    BCRYPT_SUCCESS(create_status)
	        ? BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(value.data())),
	                         static_cast<ULONG>(value.size()), 0)
	        : create_status;
	const auto finish_status =
	    BCRYPT_SUCCESS(hash_status)
	        ? BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0)
	        : hash_status;
	if (hash != nullptr) {
		BCryptDestroyHash(hash);
	}
	BCryptCloseAlgorithmProvider(algorithm, 0);
	if (!BCRYPT_SUCCESS(create_status) || !BCRYPT_SUCCESS(hash_status) ||
	    !BCRYPT_SUCCESS(finish_status)) {
		throw Error(ErrorCode::authentication, "SHA-256 operation failed");
	}
	return digest;
}
#endif

int Base64UrlValue(char ch) {
	if (ch >= 'A' && ch <= 'Z') {
		return ch - 'A';
	}
	if (ch >= 'a' && ch <= 'z') {
		return ch - 'a' + 26;
	}
	if (ch >= '0' && ch <= '9') {
		return ch - '0' + 52;
	}
	if (ch == '-' || ch == '+') {
		return 62;
	}
	if (ch == '_' || ch == '/') {
		return 63;
	}
	return -1;
}

} // namespace

std::vector<std::uint8_t> Sha256(std::string_view value) {
#ifdef _WIN32
	return BCryptDigest({}, value, false);
#else
	(void)value;
	throw Error(ErrorCode::unsupported, "SHA-256 is not available on this platform");
#endif
}

std::vector<std::uint8_t> HmacSha256(std::string_view secret, std::string_view value) {
#ifdef _WIN32
	return BCryptDigest(secret, value, true);
#else
	(void)secret;
	(void)value;
	throw Error(ErrorCode::unsupported, "HS256 is not available on this platform");
#endif
}

std::string Base64Encode(std::span<const std::uint8_t> value) {
	static constexpr char alphabet[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	return Base64(value, alphabet, true);
}

std::string Base64UrlEncode(std::span<const std::uint8_t> value) {
	static constexpr char alphabet[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	return Base64(value, alphabet, false);
}

std::string Base64UrlEncode(std::string_view value) {
	return Base64UrlEncode(
	    std::span(reinterpret_cast<const std::uint8_t*>(value.data()), value.size()));
}

std::vector<std::uint8_t> Base64UrlDecode(std::string_view value) {
	std::vector<std::uint8_t> output;
	output.reserve((value.size() * 3) / 4);
	std::uint32_t buffer = 0;
	int bits = 0;
	for (const char ch : value) {
		if (ch == '=') {
			break;
		}
		const int decoded = Base64UrlValue(ch);
		if (decoded < 0) {
			throw Error(ErrorCode::authentication, "JWT contains invalid base64url");
		}
		buffer = (buffer << 6) | static_cast<std::uint32_t>(decoded);
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			output.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xff));
		}
	}
	if (bits >= 6 || value.size() % 4 == 1) {
		throw Error(ErrorCode::authentication, "JWT contains malformed base64url");
	}
	return output;
}

bool ConstantTimeEqual(std::span<const std::uint8_t> left,
                       std::span<const std::uint8_t> right) noexcept {
	if (left.size() != right.size()) {
		return false;
	}
	std::uint8_t difference = 0;
	for (std::size_t index = 0; index < left.size(); ++index) {
		difference |= left[index] ^ right[index];
	}
	return difference == 0;
}

bool ConstantTimeEqual(std::string_view left, std::string_view right) noexcept {
	return ConstantTimeEqual(
	    std::span(reinterpret_cast<const std::uint8_t*>(left.data()), left.size()),
	    std::span(reinterpret_cast<const std::uint8_t*>(right.data()), right.size()));
}

} // namespace livekit::server::detail
