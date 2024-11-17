#ifndef PNGSQ_HEAD_HPP
#define PNGSQ_HEAD_HPP

#include <cstddef>
#include <cstdint>
#include <functional>

#ifdef _WIN32
#define STBI_WINDOWS_UTF8
#endif // _WIN32

#include <stb_image.h>
#include <libdeflate.h>

struct rgb_t;
struct img_t;
struct cfg_t;

// image.cpp

void preprocess_image(img_t& image, const cfg_t& config);
void process_image(img_t& image, const cfg_t& config);

// file.cpp

img_t load_image(char const* path);
bool write_image(const img_t& image, char const* path, libdeflate_compressor* compressor);

#pragma pack(push, 1)
// Hopefully the compiler won't decide to insert padding even if it doesn't support this `#pragma`
struct rgb_t {
	stbi_uc r, g, b;
};
#pragma pack(pop)

inline bool operator==(const rgb_t& left, const rgb_t& right) {
	return left.r == right.r && left.g == right.g && left.b == right.b;
}

inline bool operator!=(const rgb_t& left, const rgb_t& right) {
	return left.r != right.r || left.g != right.g || left.b != right.b;
}

struct img_t {
	stbi_uc* data;
	int width, height;
	rgb_t palette[16];
};

inline void free_image(img_t& image) {
	stbi_image_free(image.data);
}

struct cfg_t {
	double thr_diff_h;
	double thr_h_s;
	double thr_h_v;
	double thr_diff_s;
	double thr_diff_v;
	bool more_bg;
	double thr_more_v;
	bool dark;
	bool clear_bg;
	rgb_t override_bg;
	uint32_t sampled;
	uint8_t iterations;
};

#endif // PNGSQ_HEAD_HPP
