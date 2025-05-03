#ifndef PNGSQ_HEAD_HPP
#define PNGSQ_HEAD_HPP

#include <cstddef>
#include <cstdint>
#include <functional>

#ifdef _WIN32
#define STBI_WINDOWS_UTF8
#endif // _WIN32

#include "stb_image.h"
#include "libdeflate.h"

struct rgb;
struct img;
struct cfg;

// image.cpp

void preprocess_image(struct img& image, const struct cfg& config);
void process_image(struct img& image, const struct cfg& config);

// file.cpp

struct img load_image(char const* path);
void free_image(struct img& image);
bool write_image(const struct img& image, char const* path, libdeflate_compressor* compressor);

// Structs

#pragma pack(push, 1)
// Hopefully the compiler won't decide to insert padding even if it doesn't support `#pragma pack`
struct rgb {
	stbi_uc r, g, b;
};
#pragma pack(pop)

static inline bool operator==(const struct rgb& left, const struct rgb& right) {
	return left.r == right.r && left.g == right.g && left.b == right.b;
}

static inline bool operator!=(const struct rgb& left, const struct rgb& right) {
	return left.r != right.r || left.g != right.g || left.b != right.b;
}

struct img {
	stbi_uc* data;
	int width, height;
	struct rgb palette[16];
};

struct cfg {
	double thr_diff_h;
	double thr_h_s;
	double thr_h_v;
	double thr_diff_s;
	double thr_diff_v;
	bool more_bg;
	double thr_more_v;
	bool dark;
	bool clear_bg;
	struct rgb override_bg;
	uint32_t sampled;
	uint8_t iterations;
};

#endif // PNGSQ_HEAD_HPP
