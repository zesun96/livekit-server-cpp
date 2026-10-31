#include "livekit/server/c_api.h"
#include <cstring>

#ifdef _WIN32
#define LKS_RUBY_EXPORT __declspec(dllexport)
#else
#define LKS_RUBY_EXPORT __attribute__((visibility("default")))
#endif

namespace {
using Callback = const char* (*)(const char*, std::size_t);
struct Context {
	Callback callback;
};
int32_t Exchange(void* user, const char* input, std::size_t length, const char** output,
                 std::size_t* size) noexcept {
	try {
		*output = static_cast<Context*>(user)->callback(input, length);
		if (!*output)
			return 1;
		*size = std::strlen(*output);
		return 0;
	} catch (...) {
		return 1;
	}
}
} // namespace

// Compatibility symbols for existing Ruby gems; all operations use the common C ABI.
extern "C" {
LKS_RUBY_EXPORT int lks_ruby_abi_version() noexcept { return lks_abi_version(); }
LKS_RUBY_EXPORT void lks_ruby_free(void* output) noexcept { lks_free(output); }
LKS_RUBY_EXPORT void* lks_ruby_invoke(const char* input, std::size_t length,
                                      Callback callback) noexcept {
	Context context{callback};
	return lks_invoke(input, length, callback ? Exchange : nullptr, &context, nullptr);
}
}
