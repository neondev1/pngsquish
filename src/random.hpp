#ifndef PNGSQ_RANDOM_HPP
#define PNGSQ_RANDOM_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <random>

// Based on `mix64variant13` from Steele et al. (2014)
static constexpr uint64_t mix64(uint64_t input) {
	input = (input ^ (input >> 30)) * 0xbf58476d1ce4e5b9ULL;
	input = (input ^ (input >> 27)) * 0x94d049bb133111ebULL;
	return input ^ (input >> 31);
}

// Returns a random number on [0, UINT64_MAX]
uint64_t mix64_rand(void) {
	static std::random_device rd;
	static std::chrono::high_resolution_clock::duration time = std::chrono::high_resolution_clock::now().time_since_epoch();
	static uint64_t state = rd.entropy() ? rd()
		: std::chrono::duration_cast<std::chrono::seconds>(time).count() ^ std::chrono::duration_cast<std::chrono::milliseconds>(time).count()
		^ std::chrono::duration_cast<std::chrono::microseconds>(time).count() ^ std::chrono::duration_cast<std::chrono::nanoseconds>(time).count();
	return mix64(state += 0x9e3779b97f4a7c15ULL); // ((sqrt5-1)/2) * 2^64
}

// Returns a random number on [0, n)
uint64_t mix64_rand(uint64_t n) {
	uint64_t limit = UINT64_MAX - UINT64_MAX % n;
	uint64_t result;
	while ((result = mix64_rand()) > limit);
	return result % n;
}

// Returns a weighted random number on [0, n) if all probabilities add to 1, and returns on [0, n] otherwise
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

// Returns a number on (0, 1) weighted according to the probability density function given by `pdf`
double mix64_rand_pdf(double mode, std::function<double(double)> pdf) {
	double x, y;
	do {
		x = (double)mix64_rand() / (double)UINT64_MAX;
		y = (double)mix64_rand() * pdf(mode) / (double)UINT64_MAX;
	} while (y > pdf(x));
	return x;
}


#endif // PNGSQ_RANDOM_HPP
