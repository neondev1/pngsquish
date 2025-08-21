#ifndef PNGSQ_DEFS_HPP
#define PNGSQ_DEFS_HPP

#include <cfloat>
#include <cstring>
#include <new>

#include "glad/glad.h"

#define PNGSQ_GLSL_VERSION_STRING "#version 140"
#define PNGSQ_ERROR_STRING "pngsquish error: "

struct rgb { unsigned char r, g, b; };
static_assert(sizeof(rgb[2]) == 6);

static constexpr bool operator==(const struct rgb& left, const struct rgb& right) {
	return left.r == right.r && left.g == right.g && left.b == right.b;
}

static inline struct rgb* bytes_to_rgb(unsigned char* bytes, size_t count = 1) {
#ifdef _MSC_VER
	static_assert(_MSVC_LANG >= 202002L);
#else
	static_assert(__cplusplus >= 202002L);
#endif // _MSC_VER
	return std::launder(static_cast<struct rgb*>(std::memmove(bytes, bytes, count * sizeof(struct rgb))));
}

struct point { float x, y; };
static constexpr bool operator==(const struct point& left, const struct point& right) { return left.x == right.x && left.y == right.y; }
static constexpr struct point invalid_point = { -FLT_MAX, 0 };
// Represents a quadrilateral in normalized (on [0, 1]) coordinates
struct quad { struct point p[4]; };

struct image {
	unsigned char* data_orig;
	unsigned char* data_dewarp;
	unsigned char* data_output;
	char* path;
	int width, height;
	int out_width, out_height;
	int full_width, full_height;
	struct rgb palette[16];
	struct quad dewarp_src;
	GLuint texture;
};

static inline GLuint create_texture(unsigned char const* data, int width, int height, bool alpha) {
	GLint format = alpha ? GL_RGBA : GL_RGB;
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	return tex;
}

#endif // PNGSQ_DEFS_HPP
