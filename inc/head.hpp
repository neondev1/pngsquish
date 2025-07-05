#ifndef PNGSQ_HEAD_HPP
#define PNGSQ_HEAD_HPP

#include <cstddef>
#include <cstring>
#include <functional>
#include <new>
#include <vector>

#include "glad/glad.h"

struct libdeflate_compressor;

struct rgb;
struct image;
struct config;
struct threshold;

bool load_image(struct image& img, char const* path);
bool load_image_preview(struct image& img, char const* path, const struct config& cfg);
void free_image(struct image& img);
bool write_image(const struct image& img, char const* path, struct libdeflate_compressor* compressor, const struct config& cfg);
void preprocess_image(struct image& img, const std::vector<struct threshold>& thrs, const struct config& cfg);
void process_image(struct image& img, const struct config& cfg);

#pragma pack(push, 1)
struct rgb {
	unsigned char r, g, b;
};
#pragma pack(pop)

static inline struct rgb* bytes_to_rgb(unsigned char* bytes, size_t count = 1) {
#ifdef _MSC_VER
	static_assert(_MSVC_LANG >= 202002L);
#else
	static_assert(__cplusplus >= 202002L);
#endif // _MSC_VER
	return std::launder((struct rgb*)std::memmove(bytes, bytes, count * 3));
}

static inline bool operator==(const struct rgb& left, const struct rgb& right) {
	return left.r == right.r && left.g == right.g && left.b == right.b;
}

static inline bool operator!=(const struct rgb& left, const struct rgb& right) {
	return left.r != right.r || left.g != right.g || left.b != right.b;
}

struct hsv {
	float h, s, v;
};

struct image {
	unsigned char* data[3];
	GLuint tex0, tex1;
	int width, height;
	struct rgb palette[16];
};

#define PNGSQ_PREVIEW_NONE      0
#define PNGSQ_PREVIEW_ORIGINAL  1
#define PNGSQ_PREVIEW_DEWARPED  2
#define PNGSQ_PREVIEW_PROCESSED 3

struct config {
	int width, height;
	int prev_width, prev_height;
	bool dark, auto_palette;
	bool ovr_bg_before, ovr_bg_after;
	struct rgb ovr_bg_before_col, ovr_bg_after_col;
	int sampled, iters, prev_sampled, prev_iters;
	int prev_stage, prev_kbytes, prev_interval;
};

struct threshold {
	bool selected;
	struct hsv diff;
	bool mode;
	bool enabled;
};

#endif // PNGSQ_HEAD_HPP
