#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include "head.hpp"
#include "random.hpp"

struct hsv_t {
	double h, s, v;
};

static hsv_t to_hsv(const stbi_uc r, const stbi_uc g, const stbi_uc b) {
	const double _r = r / 255.0, _g = g / 255.0, _b = b / 255.0;
	double v = std::max({_r, _g, _b});
	double d = v - std::min({_r, _g, _b});
	double s = v ? (d / v) : 0.0;
	double h = 0.0;
	if (d) h = (v == _r ? (_g - _b) / d : v == _g ? (_b - _r) / d + 2.0 : (_r - _g) / d + 4.0) * 60.0;
	if (h > 360.0) h -= 360.0;
	return { h, s, v };
}

// Deterministic component of image processing
void preprocess_image(img_t& image, const cfg_t& config) {
	const size_t size = (size_t)image.width * (size_t)image.height * 3;
	stbi_uc* img = image.data;
	std::map<uint32_t, size_t> counts;
	stbi_uc r = 0, g = 0, b = 0;
	size_t max = 0;
	for (size_t i = 0; i < size; i += 3) {
		const size_t& count = ++counts[(img[i + 2] << 16) | (img[i + 1] << 8) | img[i]];
		if (count > max) {
			max = count;
			r = img[i + 0];
			g = img[i + 1];
			b = img[i + 2];
		}
	}
	const hsv_t background = to_hsv(r, g, b);
	if (config.clear_bg) {
		r = config.override_bg.r;
		g = config.override_bg.g;
		b = config.override_bg.b;
	}
	for (size_t i = 0; i < size; i += 3) {
		const hsv_t hsv = to_hsv(img[i], img[i + 1], img[i + 2]);
		if ((std::abs(hsv.v - background.v) < config.thr_diff_v && std::abs(hsv.s - background.s) < config.thr_diff_s
			&& !(std::abs(hsv.h - background.h) >= config.thr_diff_h
				&& hsv.v >= config.thr_h_v && hsv.s >= config.thr_h_s))
			|| (config.more_bg && (config.dark ? hsv.v < background.v + config.thr_more_v : hsv.v > background.v - config.thr_more_v) 
				&& (std::abs(hsv.s - background.s) < config.thr_diff_s || std::abs(hsv.h - background.h) < config.thr_diff_h))) {
			img[i + 0] = r;
			img[i + 1] = g;
			img[i + 2] = b;
		}
	}
	image.palette[0] = { r, g, b };
}

static inline double random(void) {
	return (mix64_rand() + 1.0) / (UINT64_MAX + 2.0);
}

static inline std::function<double(double)> beta_func(const double alpha, const double beta) {
	return [alpha, beta](double x) {
		// Use `lgamma` with `exp` as `tgamma` will overflow
		return std::pow(x, alpha - 1) * std::pow(1 - x, beta - 1) * std::exp(std::lgamma(alpha + beta) - std::lgamma(alpha) - std::lgamma(beta));
	};
}

static constexpr double sq_dist(double x1, double y1, double z1, double x2, double y2, double z2) {
	const double x = x2 - x1, y = y2 - y1, z = z2 - z1;
	return x * x + y * y + z * z;
}

static constexpr double sq_dist(const rgb_t& left, const rgb_t& right) {
	return sq_dist(left.r, left.g, left.b, right.r, right.g, right.b);
}

// Reservoir sampling, based on Algorithm M from Li (1994)
static inline void res_sample(rgb_t** sample, size_t n, rgb_t* const* colours, size_t size) {
	const int64_t r = (int64_t)(2.07 * std::sqrt(n));
	const int64_t c = (int64_t)std::floor(10.5 * (3.14245 + r) / std::log((n + r) / (n - 1.0)) - n);
	std::memcpy(sample, colours, n * sizeof(rgb_t*));
	double w = 0.0, t = 0.0, u = random();
	size_t index = n;
	for (const size_t end = n + c; index < end; index++) {
		if (index >= size)
			return;
		u *= 1.0 + (double)n / ++t;
		if (u >= 1.0) {
			sample[mix64_rand(n)] = colours[index];
			u = random();
		}
	}
	w = mix64_rand_pdf((n - 1.0) / (n + c - 1.0), beta_func((double)n, c + 1.0));
	while (1) {
		double q = std::log(1.0 - w);
		int64_t s = (int64_t)std::floor(std::log(u) / q);
		index += s + 1;
		if (index >= size)
			return;
		sample[mix64_rand(n)] = colours[index];
		t = 0.0;
		u = random();
		for (int64_t i = 0; i < r; i++) {
			s = (int64_t)std::floor(std::log(random()) / q);
			index += s + 1;
			if (index >= size)
				return;
			u *= 1 + n / ++t;
			if (u >= 1.0) {
				sample[mix64_rand(n)] = colours[index];
				u = random();
			}
		}
		w *= mix64_rand_pdf((n - 1.0) / (n + r - 1.0), beta_func((double)n, r + 1.0));
	}
}

// k-means++, based on Arthur and Vassilvitskii (2007)
static inline void k_means_pp(img_t& image, rgb_t** sample, size_t n) {
	double* distances = new double[n];
	std::fill_n(distances, n, 1.0);
	double total = 0.0;
	image.palette[1] = *sample[mix64_rand(n)];
	for (int i = 2; i < 16; i++) {
		total = 0.0;
		for (size_t j = 0; j < n; j++) {
			if (distances[j] != 0.0)
				distances[j] = sq_dist(*sample[j], image.palette[i - 1]);
			total += distances[j];
		}
		size_t index = 0;
		if (total != 0)
			while (distances[index = mix64_rand(n, distances, total)] == 0);
		image.palette[i] = *sample[index];
	}
	delete[] distances;
}

// k-means clustering
static inline void k_means(img_t& image, rgb_t** sample, size_t n, const cfg_t& config) {
	unsigned char* means = new unsigned char[n]();
	bool changed = true;
	for (uint8_t iterations = 0; (iterations < config.iterations || config.iterations == 0) && changed; iterations++) {
		changed = false;
		for (size_t i = 0; i < n; i++) {
			double best = DBL_MAX;
			for (unsigned char j = 1; j < 16; j++) {
				double dist = sq_dist(*sample[i], image.palette[j]);
				if (dist < best) {
					best = dist;
					if (means[i] != j)
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
			image.palette[j].r = (stbi_uc)(r_avg / count);
			image.palette[j].g = (stbi_uc)(g_avg / count);
			image.palette[j].b = (stbi_uc)(b_avg / count);
		}
	}
	delete[] means;
}

// Non-deterministic component of image processing
void process_image(img_t& image, const cfg_t& config) {
	size_t size = (size_t)image.width * (size_t)image.height * 3;
	std::vector<rgb_t*> foreground;
	for (size_t i = 0; i < size; i += 3) {
		rgb_t* colour = reinterpret_cast<rgb_t*>(&image.data[i]);
		if (*colour != image.palette[0])
			foreground.push_back(colour);
	}
	size = foreground.size();
	const size_t n = std::min(size, (size_t)config.sampled);
	rgb_t** sample = new rgb_t*[n];
	res_sample(sample, n, foreground.data(), size);
	k_means_pp(image, sample, n);
	k_means(image, sample, n, config);
	for (size_t i = 0; i < size; i++) {
		double best = DBL_MAX;
		unsigned colour = 0;
		for (unsigned char j = 1; j < 16; j++) {
			double dist = sq_dist(*foreground[i], image.palette[j]);
			if (dist < best) {
				best = dist;
				colour = j;
			}
		}
		foreground[i]->r = image.palette[colour].r;
		foreground[i]->g = image.palette[colour].g;
		foreground[i]->b = image.palette[colour].b;
	}
}
