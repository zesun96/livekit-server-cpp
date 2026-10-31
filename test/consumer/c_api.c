#include "livekit/server/c_api.h"
#include <string.h>

int main(void) {
	size_t size = 0;
	void* result = lks_invoke("{", 1, NULL, NULL, &size);
	int failed = lks_abi_version() != 1 || !result || size == 0 ||
	             (result && strlen((const char*)result) != size);
	lks_free(result);
	lks_free(NULL);
	return failed;
}
