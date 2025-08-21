#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "head.hpp"
#include "random.hpp"

static struct hsv to_hsv(unsigned char r, unsigned char g, unsigned char b) {
	float _r = r / 255.0f, _g = g / 255.0f, _b = b / 255.0f;
	float v = std::max({_r, _g, _b});
	float d = v - std::min({_r, _g, _b});
	float s = v ? (d / v) : 0.0f;
	float h = 0.0f;
	if (d <= 0.0f) return { h, s, v };
	else if (v == _r) h = (_g - _b) / d + 0.0f;
	else if (v == _g) h = (_b - _r) / d + 2.0f;
	else if (v == _b) h = (_r - _g) / d + 4.0f;
	if (h < 0.0f) h += 6.0f;
	h *= 60.0f;
	return { h, s, v };
}

static struct hsv hsv_diff(const struct hsv& x, const struct hsv& y) {
	float h = std::abs(x.h - y.h);
	float s = std::abs(x.s - y.s);
	float v = std::abs(x.v - y.v);
	return { std::min(h, 360.0f - h), s, v };
}

void make_background(struct image& img, const std::vector<struct threshold>& thrs, const struct config& cfg) {
	const size_t size = (size_t)3 * img.out_width * img.out_height;
	std::memcpy(img.data_output, img.data_dewarp, size);
	unsigned char* data = img.data_output;
	unsigned char r = 0, g = 0, b = 0;
	if (cfg.ovr_bg_before) {
		r = cfg.ovr_bg_before_col.r;
		g = cfg.ovr_bg_before_col.g;
		b = cfg.ovr_bg_before_col.b;
	}
	else {
		std::map<uint_fast32_t, size_t> counts;
		size_t max = 0;
		for (size_t i = 0; i < size; i += 3) {
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 6385)
#endif // _MSC_VER
			size_t count = ++counts[((uint_fast32_t)data[i + 2] << 16) | ((uint_fast32_t)data[i + 1] << 8) | data[i]];
#ifdef _MSC_VER
#	pragma warning(pop)
#endif // _MSC_VER
			if (count > max) {
				max = count;
				r = data[i + 0];
				g = data[i + 1];
				b = data[i + 2];
			}
		}
	}
	const struct hsv background = to_hsv(r, g, b);
	for (size_t i = 0; i < size; i += 3) {
		const struct hsv pixel = to_hsv(data[i], data[i + 1], data[i + 2]);
		const struct hsv diff = hsv_diff(pixel, background);
		for (const struct threshold& thr: thrs) {
			if (diff.h > thr.diff.h || diff.s > thr.diff.s || (!thr.mode && diff.v > thr.diff.v))
				continue;
			else if (thr.mode && !cfg.dark && pixel.v < background.v - thr.diff.v)
				continue;
			else if (thr.mode && cfg.dark && pixel.v > background.v + thr.diff.v)
				continue;
			if (cfg.ovr_bg_after) {
				data[i + 0] = cfg.ovr_bg_after_col.r;
				data[i + 1] = cfg.ovr_bg_after_col.g;
				data[i + 2] = cfg.ovr_bg_after_col.b;
				break;
			}
			else {
				data[i + 0] = r;
				data[i + 1] = g;
				data[i + 2] = b;
				break;
			}
		}
	}
	img.palette[0] = cfg.ovr_bg_after ? cfg.ovr_bg_after_col : rgb { r, g, b };
}

// Generates a random floating-point number on (0, 1)
static inline float random(void) {
	return (mix32_rand() + 1.0f) / (UINT32_MAX + 2.0f);
}

static inline std::function<float(float)> beta_func(const float alpha, const float beta) {
	return [alpha, beta](float x) -> float {
		// Use `lgamma` with `exp` as `tgamma` will overflow
		return std::powf(x, alpha - 1) * std::powf(1 - x, beta - 1)
			* std::expf(std::lgammaf(alpha + beta) - std::lgammaf(alpha) - std::lgammaf(beta));
	};
}

// Reservoir sampling, based on Algorithm M from Li (1994)
static inline void res_sample(struct rgb** sample, int n, struct rgb* const* colours, size_t size) {
	const int64_t r = (int64_t)(2.07 * std::sqrt(n));
	const int64_t c = (int64_t)std::floor(10.5 * (3.14245 + r) / std::log((n + r) / (n - 1.0)) - n);
	std::memcpy(sample, colours, n * sizeof(struct rgb*));
	float w = 0.0f, t = 0.0f, u = random();
	size_t index = n;
	for (const size_t end = n + c; index < end; index++) {
		if (index >= size)
			return;
		u *= 1.0f + (float)n / ++t;
		if (u >= 1.0) {
			sample[mix32_rand(n)] = colours[index];
			u = random();
		}
	}
	w = mix32_rand_pdf((n - 1.0f) / (n + c - 1.0f), beta_func((float)n, c + 1.0f));
	while (1) {
		float q = std::logf(1.0f - w);
		int64_t s = (int64_t)std::floor(std::logf(u) / q);
		index += s + 1;
		if (index >= size)
			return;
		sample[mix32_rand(n)] = colours[index];
		t = 0.0f;
		u = random();
		for (int64_t i = 0; i < r; i++) {
			s = (int64_t)std::floor(std::logf(random()) / q);
			index += s + 1;
			if (index >= size)
				return;
			u *= 1.0f + n / ++t;
			if (u >= 1.0f) {
				sample[mix32_rand(n)] = colours[index];
				u = random();
			}
		}
		w *= mix32_rand_pdf((n - 1.0f) / (n + r - 1.0f), beta_func((float)n, r + 1.0f));
	}
}

static constexpr float dist2(const struct rgb& left, const struct rgb& right) {
	const float dr = left.r - right.r, dg = left.g - right.g, db = left.b - right.b;
	return dr * dr + dg * dg + db * db;
}

// k-means++, based on Arthur and Vassilvitskii (2007)
static inline void k_means_pp(struct image& img, struct rgb** sample, int n) {
	auto distances = std::make_unique<float[]>(n);
	std::memset(distances.get(), 1, n * sizeof(float)); // fill with any nonzero values (can be junk)
	img.palette[1] = *sample[mix32_rand(n)];
	for (int entry = 2; entry < 16; entry++) {
		float total = 0.0;
		for (int i = 0; i < n; i++) {
			if (distances[i] == 0.0)
				continue;
			distances[i] = dist2(*sample[i], img.palette[entry - 1]);
			total += distances[i];
		}
		int index = 0;
		if (total != 0.0)
			while (distances[index = (int)mix32_rand(n, distances.get(), total)] == 0);
		img.palette[entry] = *sample[index];
	}
}

// k-means clustering
static inline void k_means(struct image& img, struct rgb** sample, int n, const struct config& cfg) {
	auto means = std::make_unique<unsigned char[]>(n);
	bool changed = true;
	for (int iterations = 0; iterations < cfg.iters && changed; iterations++) {
		changed = false;
		for (int i = 0; i < n; i++) {
			float best = FLT_MAX;
			for (int entry = 1; entry < 16; entry++) {
				float dist = dist2(*sample[i], img.palette[entry]);
				if (best > dist) {
					best = dist;
					if (means[i] == entry)
						continue;
					changed = true;
					means[i] = entry;
				}
			}
		}
		for (int entry = 1; entry < 16; entry++) {
			uint64_t r_sum = 0, g_sum = 0, b_sum = 0, count = 0;
			for (size_t i = 0; i < n; i++) {
				if (means[i] == entry) {
					r_sum += sample[i]->r;
					g_sum += sample[i]->g;
					b_sum += sample[i]->b;
					count++;
				}
			}
			if (count == 0)
				continue;
			img.palette[entry].r = (unsigned char)std::roundf((float)r_sum / count);
			img.palette[entry].g = (unsigned char)std::roundf((float)g_sum / count);
			img.palette[entry].b = (unsigned char)std::roundf((float)b_sum / count);
		}
	}
}

void make_palette(struct image& img, const struct config& cfg) {
	size_t size = (size_t)img.out_width * (size_t)img.out_height;
	struct rgb* pxs = bytes_to_rgb(img.data_output, size);
	std::vector<struct rgb*> foreground;
	for (size_t i = 0; i < size; i++) {
		struct rgb* colour = &pxs[i];
		if (*colour != img.palette[0])
			foreground.push_back(colour);
	}
	size = foreground.size();
	const int n = (int)std::min(size, (size_t)cfg.sampled);
	auto sample = std::make_unique<struct rgb*[]>(n);
	res_sample(sample.get(), n, foreground.data(), size);
	k_means_pp(img, sample.get(), n);
	k_means(img, sample.get(), n, cfg);
	for (size_t i = 0; i < size; i++) {
		struct rgb& px = *foreground[i];
		float best = FLT_MAX;
		int colour = 0;
		for (int entry = 0; entry < 16; entry++) {
			float dist = dist2(px, img.palette[entry]);
			if (best > dist) {
				best = dist;
				colour = entry;
			}
		}
		px.r = img.palette[colour].r;
		px.g = img.palette[colour].g;
		px.b = img.palette[colour].b;
	}
}

void use_palette(struct image& img, const struct config& cfg) {
	size_t size = (size_t)img.out_width * (size_t)img.out_height;
	struct rgb* pxs = bytes_to_rgb(img.data_output, size);
	std::vector<struct rgb*> foreground;
	for (size_t i = 0; i < size; i++) {
		struct rgb* colour = &pxs[i];
		if (*colour != img.palette[0])
			foreground.push_back(colour);
	}
	size = foreground.size();
	for (size_t i = 0; i < size; i++) {
		struct rgb& px = *foreground[i];
		float best = FLT_MAX;
		int colour = 0;
		for (int entry = 0; entry < 16; entry++) {
			float dist = dist2(px, img.palette[entry]);
			if (best > dist) {
				best = dist;
				colour = entry;
			}
		}
		px.r = img.palette[colour].r;
		px.g = img.palette[colour].g;
		px.b = img.palette[colour].b;
	}
}
