#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "head.hpp"

// Checks if a path has the .png extension
static bool has_ext(char const* path) {
	static constexpr char png[] = ".png";
	size_t len = strlen(path);
	if (len < 4)
		return false;
	char const* const end = path + len;
	char const* const begin = end - 4;
	return std::equal(begin, end, png, png + sizeof(png) - 1, [](const char& left, const char& right) { return std::toupper(left) == std::toupper(right); });
}

int main(void) {
	libdeflate_compressor* compressor = libdeflate_alloc_compressor(12);
	if (!compressor)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
