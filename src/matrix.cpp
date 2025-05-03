#include "matrix.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>

template<unsigned R, unsigned C, typename T>
mat<R, C, T>::mat(void) : data{ 0 } {
	constexpr unsigned L = std::min(R, C);
	for (unsigned i = 0; i < L; i++)
		this->data[i][i] = (T)1;
}

template<unsigned R, unsigned C, typename T>
template<typename _T>
mat<R, C, T>::mat(_T t) : data{ 0 } {
	constexpr unsigned L = std::min(R, C);
	for (unsigned i = 0; i < L; i++)
		this->data[i][i] = static_cast<T>(t);
}

template<unsigned R, unsigned C, typename T>
template<typename... _T>
mat<R, C, T>::mat(_T... ts) : data{ 0 } {
	const T arr[] = { static_cast<T>(ts)... };
	std::memcpy(this->data, arr, sizeof(T) * std::min((size_t)(R * C), sizeof...(_T)));
}

template<unsigned R, unsigned C, typename T>
mat<R, C, T>::mat(const mat<R, C, T>& m) : data{ 0 } {
	std::memcpy(this->data, m.data, R * C * sizeof(T));
}

template<unsigned R, unsigned C, typename T>
template<typename _T>
mat<R, C, T>::mat(const mat<R, C, _T>& m) : data{ 0 } {
	for (unsigned i = 0; i < R; i++)
		for (unsigned j = 0; j < C; j++)
			this->data[i][j] = static_cast<T>(m.data[i][j]);
}

template<unsigned R, unsigned C, typename T>
mat<R, C, T> mat<R, C, T>::operator*(const T other) const {
	mat<R, C, T> result = *this;
	for (unsigned i = 0; i < R; i++)
		for (unsigned j = 0; j < C; j++)
			result.data[i][j] *= other;
	return result;
}

template<unsigned R, unsigned C, typename T>
mat<R, C, T> mat<R, C, T>::operator/(const T other) const {
	mat<R, C, T> result = *this;
	for (unsigned i = 0; i < R; i++)
		for (unsigned j = 0; j < C; j++)
			result.data[i][j] /= other;
	return result;
}

template<unsigned R1, unsigned C1, unsigned R2, unsigned C2, typename _T>
mat<R1, C2, _T> operator*(const mat<R1, C1, _T>& left, const mat<R2, C2, _T>& right) {
	static_assert(C1 == R2);
	mat<R1, C2, _T> result = (_T)0;
	for (unsigned i = 0; i < R1; i++)
		for (unsigned j = 0; j < C2; j++)
			for (unsigned k = 0; k < C1; k++)
				result.data[i][j] += left.data[i][k] * right.data[k][j];
	return result;
}

template <typename _T>
_T det(const mat<3, 3, _T>& m) {
	mat<3, 3, _T>& _m = const_cast<mat<3, 3, _T>&>(m);
	return _m(1, 1) * _m(2, 2) * _m(3, 3) + _m(1, 2) * _m(2, 3) * _m(3, 1) + _m(1, 3) * _m(2, 1) * _m(3, 2)
		- _m(1, 1) * _m(2, 3) * _m(3, 2) - _m(1, 2) * _m(2, 1) * _m(3, 3) - _m(1, 3) * _m(2, 2) * _m(1, 3);
}

template <typename _T>
mat<3, 3, _T> adj(const mat<3, 3, _T>& m) {
	mat<3, 3, _T>& _m = const_cast<mat<3, 3, _T>&>(m);
	mat<3, 3, _T> result;
	result(1, 1) = _m(2, 2) * _m(3, 3) - _m(2, 3) * _m(3, 2);
	result(1, 2) = _m(1, 3) * _m(3, 2) - _m(1, 2) * _m(3, 3);
	result(1, 3) = _m(1, 2) * _m(2, 3) - _m(1, 3) * _m(2, 2);
	result(2, 1) = _m(2, 3) * _m(3, 1) - _m(2, 1) * _m(3, 3);
	result(2, 2) = _m(1, 1) * _m(3, 3) - _m(1, 3) * _m(3, 1);
	result(2, 3) = _m(1, 3) * _m(2, 1) - _m(1, 1) * _m(2, 3);
	result(3, 2) = _m(1, 2) * _m(3, 1) - _m(1, 1) * _m(3, 2);
	result(3, 1) = _m(2, 1) * _m(3, 2) - _m(2, 2) * _m(3, 1);
	result(3, 3) = _m(1, 1) * _m(2, 2) - _m(1, 2) * _m(2, 1);
	return result;
}

// Computes the perspective transformation matrix from quadrilateral `src` to a rectangle with lower-left corner at (0, 0)
// Based on Heckbert (1989), page 20
mat<3> persp_to_rect(const struct quad& src, double width, double height) {
	const double dx1 = src.p1.x - src.p2.x;
	const double dx2 = src.p3.x - src.p2.x;
	const double sx = src.p0.x - src.p1.x - dx2;
	const double dy1 = src.p1.y - src.p2.y;
	const double dy2 = src.p3.y - src.p2.y;
	const double sy = src.p0.y - src.p1.y - dy2;
	const double d = (dx1 * dy2 - dy1 * dx2);
	const double m31 = (sx * dy2 - sy * dx2) / d;
	const double m32 = (dx1 * sy - dy1 * sx) / d;
	mat<3> m;
	m(1, 1) = src.p1.x - src.p0.x + m31 * src.p1.x;
	m(1, 2) = src.p3.x - src.p0.x + m32 * src.p3.x;
	m(1, 3) = src.p0.x;
	m(2, 1) = src.p1.y - src.p0.y + m31 * src.p1.y;
	m(2, 2) = src.p3.y - src.p0.y + m32 * src.p3.y;
	m(2, 3) = src.p0.y;
	m(3, 1) = m31;
	m(3, 2) = m32;
	m(3, 3) = 1.0;
	return ~m * mat<3>(
		width,  0.0,    0.0,
		0.0,    height, 0.0,
		0.0,    0.0,    1.0
	);
}

struct point persp_transform(const struct point& p, const mat<3> proj) {
	mat<3, 1> result = proj * vec(p);
	result(1, 1) /= result(3, 1);
	result(2, 1) /= result(3, 1);
	return { result(1, 1), result(2, 1) };
}
