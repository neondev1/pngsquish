#ifndef PNGSQ_MATRIX_HPP
#define PNGSQ_MATRIX_HPP

#include <algorithm>
#include <cstddef>
#include <cstring>

template<unsigned R, unsigned C = R, typename T = double>
class mat {
	template<unsigned, unsigned, typename>
	friend class mat;
	template<unsigned R1, unsigned C1, unsigned R2, unsigned C2, typename _T>
	friend inline mat<R1, C2, _T> operator*(const mat<R1, C1, _T>& left, const mat<R2, C2, _T>& right);
protected:
	T data[R][C];
public:
	mat(void);
	template<typename _T>
	mat(_T t);
	template<typename... _T>
	mat(_T... ts);
	mat(const mat<R, C, T>& m);
	template<typename _T>
	mat(const mat<R, C, _T>& m);

	constexpr T& operator()(unsigned row, unsigned column);

	mat<R, C, T> operator*(const T other) const;
	mat<R, C, T> operator/(const T other) const;
};

template<unsigned R, unsigned C, typename T>
constexpr T& mat<R, C, T>::operator()(const unsigned row, const unsigned column) {
	return data[row - 1][column - 1];
}

template<unsigned R1, unsigned C1, unsigned R2, unsigned C2, typename _T>
mat<R1, C2, _T> operator*(const mat<R1, C1, _T>& left, const mat<R2, C2, _T>& right);

template <typename _T>
_T det(const mat<3, 3, _T>& m);
template <typename _T>
mat<3, 3, _T> adj(const mat<3, 3, _T>& m);

template<typename _T>
static inline mat<3, 3, _T> operator~(const mat<3, 3, _T>& m) {
	return adj(m) / det(m);
}

struct point { double x, y; };
struct quad { struct point p0, p1, p2, p3; };

static inline mat<3, 1> vec(const struct point p) {
	return mat<3, 1>(p.x, p.y, 1.0);
}

// Computes the perspective transformation matrix from quadrilateral `src` to a rectangle with lower-left corner at (0, 0)
// Based on Heckbert (1989), page 20
mat<3> persp_to_rect(const struct quad& src, double width, double height);
struct point persp_transform(const struct point& p, const mat<3> proj);

#endif // PNGSQ_MATRIX_HPP
