#ifndef PNGSQ_HEAD_HPP
#define PNGSQ_HEAD_HPP

#include <cmath>
#include <vector>

#include "glad/glad.h"

#include "defs.hpp"

struct config;
struct threshold;
struct libdeflate_compressor;

bool load_image(struct image& img, char const* path);
bool load_image_preview(struct image& img, char const* path, const struct config& cfg);
void free_image(struct image& img);
bool write_image(const struct image& img, char const* path, struct libdeflate_compressor* compressor);
void make_background(struct image& img, const std::vector<struct threshold>& thrs, const struct config& cfg);
void make_palette(struct image& img, const struct config& cfg);
void use_palette(struct image& img, const struct config& cfg);

#define PNGSQ_PREVIEW_NONE      0
#define PNGSQ_PREVIEW_ORIGINAL  1
#define PNGSQ_PREVIEW_DEWARPED  2
#define PNGSQ_PREVIEW_PROCESSED 3

#define PNGSQ_VAL_MODE_RANGE    0
#define PNGSQ_VAL_MODE_COMPARE  1

#define PNGSQ_VERT_MOVE_NEAREST 0
#define PNGSQ_VERT_MOVE_OLDEST  1
#define PNGSQ_VERT_CLEAR_ALL    2

struct config {
	int width, height;
	int sampled, iters;
	bool dark, auto_palette;
	bool ovr_bg_before, ovr_bg_after;
	struct rgb ovr_bg_before_col, ovr_bg_after_col;
	int prev_stage, prev_kbytes, prev_interval;
	int prev_click_behaviour;
};

struct hsv { float h, s, v; };
struct threshold {
	bool selected;
	struct hsv diff;
	bool mode;
	bool enabled;
};

static inline float calc_prev_scale(int width, int height, int kb) {
	return std::sqrtf(kb / (0.02f * width * height));
}

#endif // PNGSQ_HEAD_HPP
