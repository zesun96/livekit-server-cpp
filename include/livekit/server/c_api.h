#ifndef LIVEKIT_SERVER_C_API_H
#define LIVEKIT_SERVER_C_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(LIVEKIT_SERVER_C_SHARED)
#if defined(LIVEKIT_SERVER_BUILDING_LIBRARY)
#define LKS_API __declspec(dllexport)
#else
#define LKS_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define LKS_API __attribute__((visibility("default")))
#else
#define LKS_API
#endif

#ifdef __cplusplus
#define LKS_NOEXCEPT noexcept
extern "C" {
#else
#define LKS_NOEXCEPT
#endif

/* Stable SDK error values in the JSON failure envelope. */
typedef enum lks_error_code {
	LKS_INVALID_ARGUMENT = 0,
	LKS_AUTHENTICATION = 1,
	LKS_TRANSPORT = 2,
	LKS_HTTP = 3,
	LKS_PROTOCOL = 4,
	LKS_UNSUPPORTED = 5,
	LKS_UNKNOWN = 255
} lks_error_code;

/* Synchronous callback on the invoking thread. Return 0 and set a UTF-8 JSON
 * response, or nonzero on failure. Request is borrowed only during callback.
 * Response is borrowed until lks_invoke returns; SDK never frees it.
 * No exception/longjmp may cross this boundary. user_data is never retained.
 * Nested invokes are allowed; concurrent invokes require independent or
 * synchronized callback state. No callback runs after lks_invoke returns. */
typedef int32_t (*lks_callback)(void* user_data, const char* request, size_t request_length,
                                const char** response, size_t* response_length);

LKS_API int32_t lks_abi_version(void) LKS_NOEXCEPT;
/* Input is UTF-8 JSON, borrowed for this call. Output is NUL-terminated UTF-8
 * JSON with an optional byte length excluding NUL. Success: {ok:true,value:...}.
 * Failure: {ok:false,code,message,http_status,twirp_code}. NULL means native
 * allocation/encoding failure; output_length is then zero. Release every
 * non-NULL result using lks_free, in the same library that allocated it.
 * See docs/c-abi.md for operations and callback payloads. */
LKS_API void* lks_invoke(const char* input, size_t length, lks_callback callback, void* user_data,
                         size_t* output_length) LKS_NOEXCEPT;
LKS_API void lks_free(void* output) LKS_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef LKS_NOEXCEPT
#endif
