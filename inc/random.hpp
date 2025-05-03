#ifndef PNGSQ_RANDOM_HPP
#define PNGSQ_RANDOM_HPP

#include <cstdint>
#include <functional>

// Returns a random integer on [0, UINT64_MAX]
uint64_t mix64_rand(void);

// Returns a random integer on [0, n)
uint64_t mix64_rand(uint64_t n);

// Returns a weighted random number on [0, n) if all probabilities add to 1
// The denominator should not exceed UINT64_MAX
uint64_t mix64_rand(uint64_t n, double const* numerators, double denominator);

// Returns a number on (0, 1) weighted according to the probability density function `pdf`
double mix64_rand_pdf(double mode, std::function<double(double)> pdf);


#endif // PNGSQ_RANDOM_HPP
