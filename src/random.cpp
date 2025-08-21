#include <cstdint>
#include <functional>
#include <random>

#include "random.hpp"

static uint64_t state;

void init_rand(void) {
	std::random_device rd;
	state = rd();
}

// `mix32` from Steele et al. (2014)
static constexpr uint32_t mix32(uint64_t input) {
	input = (input ^ (input >> 33)) * 0xff51afd7ed558ccdULL;
	input = (input ^ (input >> 33)) * 0xc4ceb9fe1a85ec53ULL;
	return (uint32_t)(input >> 32);
}

// Returns a random integer on [0, UINT32_MAX]
uint32_t mix32_rand(void) {
	return mix32(state += 0x9e3779b97f4a7c15ULL); // ((sqrt5-1)/2) * 2^64
}

// Returns a random integer on [0, n)
uint32_t mix32_rand(uint32_t n) {
	uint32_t limit = UINT32_MAX - UINT32_MAX % n;
	uint32_t result;
	while ((result = mix32_rand()) >= limit);
	return result % n;
}

// Returns a weighted random number on [0, n) if all probabilities add to 1
// The denominator should not exceed UINT32_MAX
uint32_t mix32_rand(uint32_t n, float const* numerators, float denominator) {
	float x = (float)mix32_rand((uint32_t)denominator);
	for (uint64_t i = 0; i < n; i++) {
		if (numerators[i] > x)
			return i;
		x -= numerators[i];
	}
	return n;
}

// Returns a number on (0, 1) weighted according to the probability density function `pdf`
float mix32_rand_pdf(float mode, std::function<float(float)> pdf) {
	float x, y;
	do {
		x = (float)mix32_rand() / (float)UINT32_MAX;
		y = (float)mix32_rand() * pdf(mode) / (float)UINT32_MAX;
	} while (y > pdf(x));
	return x;
}

// `mix64variant13` from Steele et al. (2014)
static constexpr uint64_t mix64(uint64_t input) {
	input = (input ^ (input >> 30)) * 0xbf58476d1ce4e5b9ULL;
	input = (input ^ (input >> 27)) * 0x94d049bb133111ebULL;
	return input ^ (input >> 31);
}

// Returns a random integer on [0, UINT64_MAX]
uint64_t mix64_rand(void) {
	return mix64(state += 0x9e3779b97f4a7c15ULL); // ((sqrt5-1)/2) * 2^64
}

// Returns a random integer on [0, n)
uint64_t mix64_rand(uint64_t n) {
	uint64_t limit = UINT64_MAX - UINT64_MAX % n;
	uint64_t result;
	while ((result = mix64_rand()) >= limit);
	return result % n;
}

// Returns a weighted random number on [0, n) if all probabilities add to 1
// The denominator should not exceed UINT64_MAX
uint64_t mix64_rand(uint64_t n, double const* numerators, double denominator) {
	double x = (double)mix64_rand((uint64_t)denominator);
	for (uint64_t i = 0; i < n; i++) {
		if (numerators[i] > x)
			return i;
		x -= numerators[i];
	}
	return n;
}

// Returns a number on (0, 1) weighted according to the probability density function `pdf`
double mix64_rand_pdf(double mode, std::function<double(double)> pdf) {
	double x, y;
	do {
		x = (double)mix64_rand() / (double)UINT64_MAX;
		y = (double)mix64_rand() * pdf(mode) / (double)UINT64_MAX;
	} while (y > pdf(x));
	return x;
}
