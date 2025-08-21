#ifndef PNGSQ_MATRIX_HPP
#define PNGSQ_MATRIX_HPP

#include <algorithm>
#include <cstring>

template<unsigned R, unsigned C = R, typename T = float>
class mat {
	template<unsigned, unsigned, typename>
	friend class mat;
protected:
	T data[R][C];
public:
	inline mat(void) noexcept;
	template<typename _T>
	inline mat(_T t) noexcept;
	template<typename... _T>
	inline mat(_T... ts) noexcept;
	inline mat(const mat<R, C, T>& m) noexcept;
	inline mat(mat<R, C, T>&& m) noexcept = default;
	template<typename _T>
	inline mat(const mat<R, C, _T>& m) noexcept;

	constexpr T& operator()(unsigned row, unsigned column);
	constexpr const T& operator()(unsigned row, unsigned column) const;
	constexpr T const* ptr(void) const;

	inline mat<R, C, T> operator*(const T other) const;
	inline mat<R, C, T> operator/(const T other) const;
};

template<unsigned R, unsigned C, typename T>
inline mat<R, C, T>::mat(void) noexcept : data{0} {
	constexpr unsigned L = std::min(R, C);
	for (unsigned i = 0; i < L; i++)
		this->data[i][i] = (T)1;
}

template<unsigned R, unsigned C, typename T>
template<typename _T>
inline mat<R, C, T>::mat(_T t) noexcept : data{0} {
	constexpr unsigned L = std::min(R, C);
	for (unsigned i = 0; i < L; i++)
		this->data[i][i] = static_cast<T>(t);
}

template<unsigned R, unsigned C, typename T>
template<typename... _T>
inline mat<R, C, T>::mat(_T... ts) noexcept : data{0} {
	constexpr unsigned L = std::min(R, C);
	for (unsigned i = 0; i < L; i++)
		this->data[i][i] = (T)1;
	const T arr[] = { static_cast<T>(ts)... };
	std::memcpy(this->data, arr, sizeof(T) * std::min((size_t)R * (size_t)C, sizeof...(_T)));
}

template<unsigned R, unsigned C, typename T>
inline mat<R, C, T>::mat(const mat<R, C, T>& m) noexcept : data{0} {
	std::memcpy(this->data, m.data, sizeof(T) * R * C);
}

template<unsigned R, unsigned C, typename T>
template<typename _T>
inline mat<R, C, T>::mat(const mat<R, C, _T>& m) noexcept : data{0} {
	for (unsigned i = 0; i < R; i++)
		for (unsigned j = 0; j < C; j++)
			this->data[i][j] = static_cast<T>(m.data[i][j]);
}

template<unsigned R, unsigned C, typename T>
inline mat<R, C, T> mat<R, C, T>::operator*(const T other) const {
	mat<R, C, T> result = *this;
	for (unsigned i = 0; i < R; i++)
		for (unsigned j = 0; j < C; j++)
			result.data[i][j] *= other;
	return result;
}

template<unsigned R, unsigned C, typename T>
inline mat<R, C, T> mat<R, C, T>::operator/(const T other) const {
	mat<R, C, T> result = *this;
	for (unsigned i = 0; i < R; i++)
		for (unsigned j = 0; j < C; j++)
			result.data[i][j] /= other;
	return result;
}

template<unsigned R, unsigned C, typename T>
constexpr T& mat<R, C, T>::operator()(const unsigned row, const unsigned column) {
	return data[row - 1][column - 1];
}

template<unsigned R, unsigned C, typename T>
constexpr const T& mat<R, C, T>::operator()(const unsigned row, const unsigned column) const {
	return data[row - 1][column - 1];
}

template<unsigned R, unsigned C, typename T>
constexpr T const* mat<R, C, T>::ptr(void) const {
	return &data[0][0];
}

template<unsigned R1, unsigned C1, unsigned R2, unsigned C2, typename _T>
static inline mat<R1, C2, _T> operator*(const mat<R1, C1, _T>& left, const mat<R2, C2, _T>& right) {
	static_assert(C1 == R2);
	mat<R1, C2, _T> result = (_T)0;
	for (unsigned i = 1; i <= R1; i++)
		for (unsigned j = 1; j <= C2; j++)
			for (unsigned k = 1; k <= C1; k++)
				result(i, j) += left(i, k) * right(k, j);
	return result;
}

template <typename _T>
static inline _T det(const mat<3, 3, _T>& m) {
	return m(1, 1) * m(2, 2) * m(3, 3) + m(1, 2) * m(2, 3) * m(3, 1) + m(1, 3) * m(2, 1) * m(3, 2)
		- m(1, 1) * m(2, 3) * m(3, 2) - m(1, 2) * m(2, 1) * m(3, 3) - m(1, 3) * m(2, 2) * m(1, 3);
}

template <typename _T>
static inline mat<3, 3, _T> adj(const mat<3, 3, _T>& m) {
	mat<3, 3, _T> result;
	result(1, 1) = m(2, 2) * m(3, 3) - m(2, 3) * m(3, 2);
	result(1, 2) = m(1, 3) * m(3, 2) - m(1, 2) * m(3, 3);
	result(1, 3) = m(1, 2) * m(2, 3) - m(1, 3) * m(2, 2);
	result(2, 1) = m(2, 3) * m(3, 1) - m(2, 1) * m(3, 3);
	result(2, 2) = m(1, 1) * m(3, 3) - m(1, 3) * m(3, 1);
	result(2, 3) = m(1, 3) * m(2, 1) - m(1, 1) * m(2, 3);
	result(3, 2) = m(1, 2) * m(3, 1) - m(1, 1) * m(3, 2);
	result(3, 1) = m(2, 1) * m(3, 2) - m(2, 2) * m(3, 1);
	result(3, 3) = m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1);
	return result;
}

template <typename _T>
static inline mat<3, 3, _T> operator~(const mat <3, 3, _T>& m) {
	return adj(m) / det(m);
}

#endif // PNGSQ_MATRIX_HPP
