#include "matrix.hpp"

#ifndef __APPLE__
#	define PNGSQ_GLSL_VERSION_STRING "#version 130"
#else
#	define PNGSQ_GLSL_VERSION_STRING "#version 150"
#endif // __APPLE__

struct point { float x, y; };
struct quad { struct point p0, p1, p2, p3; };

// Initializes rectangle VAO, shaders and framebuffer
bool init_shaders(void);
void deinit_shaders(void);

// Computes the perspective transformation matrix from quadrilateral `src` to a rectangle with lower-left corner at (0, 0)
// Based on Heckbert (1989), page 20
mat<3> persp_matrix(const struct quad& src);

// Uses OpenGL to transform an image using a matrix
void transform_image(struct image& img, const mat<3>& matrix, const struct config& cfg);
