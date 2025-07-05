#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "head.hpp"
#include "random.hpp"

static struct hsv to_hsv(unsigned char r, unsigned char g, unsigned char b) {
	const float _r = r / 255.0f, _g = g / 255.0f, _b = b / 255.0f;
	float v = std::max({_r, _g, _b});
	float d = v - std::min({_r, _g, _b});
	float s = v ? (d / v) : 0.0f;
	float h = 0.0f;
	if (d) h = (v == _r ? (_g - _b) / d : v == _g ? (_b - _r) / d + 2.0f : (_r - _g) / d + 4.0f) * 60.0f;
	if (h > 360.0f) h -= 360.0f;
	return { h, s, v };
}

static struct hsv hsv_diff(const struct hsv& x, const struct hsv& y) {
	const float h = std::abs(x.h - y.h);
	const float s = std::abs(x.s - y.s);
	const float v = std::abs(x.v - y.v);
	return { std::min(h, 360.0f - h), s, v };
}

void preprocess_image(struct image& img, const std::vector<struct threshold>& thrs, const struct config& cfg) {
	const size_t size = (size_t)3 * cfg.width * cfg.height;
	std::memcpy(img.data[2], img.data[1], size);
	unsigned char* data = img.data[2];
	unsigned char r = 0, g = 0, b = 0;
	if (cfg.ovr_bg_before) {
		r = cfg.ovr_bg_before_col.r;
		g = cfg.ovr_bg_before_col.g;
		b = cfg.ovr_bg_before_col.b;
	}
	else {
		std::map<uint32_t, size_t> counts;
		size_t max = 0;
		for (size_t i = 0; i < size; i += 3) {
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 6385)
#endif // _MSC_VER
			const size_t& count = ++counts[((uint32_t)data[i + 2] << 16) | ((uint32_t)data[i + 1] << 8) | data[i]];
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
		int64_t s = (int64_t)std::floor(std::log(u) / q);
		index += s + 1;
		if (index >= size)
			return;
		sample[mix32_rand(n)] = colours[index];
		t = 0.0f;
		u = random();
		for (int64_t i = 0; i < r; i++) {
			s = (int64_t)std::floor(std::log(random()) / q);
			index += s + 1;
			if (index >= size)
				return;
			u *= 1 + n / ++t;
			if (u >= 1.0) {
				sample[mix32_rand(n)] = colours[index];
				u = random();
			}
		}
		w *= mix32_rand_pdf((n - 1.0f) / (n + r - 1.0f), beta_func((float)n, r + 1.0f));
	}
}

static constexpr float dist2(float x1, float y1, float z1, float x2, float y2, float z2) {
	const float x = x2 - x1, y = y2 - y1, z = z2 - z1;
	return x * x + y * y + z * z;
}

static constexpr float dist2(const struct rgb& left, const struct rgb& right) {
	return dist2(left.r, left.g, left.b, right.r, right.g, right.b);
}

// k-means++, based on Arthur and Vassilvitskii (2007)
static inline void k_means_pp(struct image& img, struct rgb** sample, int n) {
	auto distances = std::make_unique<float[]>(n);
	std::fill_n(distances.get(), n, 1.0f);
	img.palette[1] = *sample[mix32_rand(n)];
	for (int i = 2; i < 16; i++) {
		float total = 0.0;
		for (int j = 0; j < n; j++) {
			if (distances[j] != 0.0)
				distances[j] = dist2(*sample[j], img.palette[i - 1]);
			total += distances[j];
		}
		int index = 0;
		if (total != 0.0)
			while (distances[index = (int)mix32_rand(n, distances.get(), total)] == 0);
		img.palette[i] = *sample[index];
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
			for (unsigned char j = 1; j < 16; j++) {
				float dist = dist2(*sample[i], img.palette[j]);
				if (dist < best) {
					best = dist;
					if (means[i] == j)
						continue;
					changed = true;
					means[i] = j;
				}
			}
		}
		for (unsigned char j = 1; j < 16; j++) {
			uint64_t r_avg = 0, g_avg = 0, b_avg = 0, count = 0;
			for (size_t i = 0; i < n; i++) {
				if (means[i] == j) {
					r_avg += sample[i]->r;
					g_avg += sample[i]->g;
					b_avg += sample[i]->b;
					count++;
				}
			}
			if (count == 0)
				continue;
			img.palette[j].r = (unsigned char)(r_avg / count);
			img.palette[j].g = (unsigned char)(g_avg / count);
			img.palette[j].b = (unsigned char)(b_avg / count);
		}
	}
}

void process_image(struct image& img, const struct config& cfg) {
	unsigned char* data = img.data[2];
	size_t size = (size_t)cfg.width * (size_t)cfg.height;
	struct rgb* pxs = bytes_to_rgb(data, size);
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
		double best = DBL_MAX;
		int colour = 0;
		for (int j = 0; j < 16; j++) {
			double dist = dist2(*foreground[i], img.palette[j]);
			if (dist < best) {
				best = dist;
				colour = j;
			}
		}
		foreground[i]->r = img.palette[colour].r;
		foreground[i]->g = img.palette[colour].g;
		foreground[i]->b = img.palette[colour].b;
	}
}
