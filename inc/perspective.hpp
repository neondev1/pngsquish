#ifndef PNGSQ_PERSPECTIVE_HPP
#define PNGSQ_PERSPECTIVE_HPP

#include <deque>

#include "defs.hpp"
#include "matrix.hpp"

// Initializes VAO, shaders and framebuffer
bool init_shaders_persp(void);
void deinit_shaders_persp(void);

// Computes the perspective transformation matrix from quadrilateral `img.dewarp_src` to a 2x2 square centred at (0, 0)
// Based on Heckbert (1989), page 20
mat<3> persp_matrix(const struct image& img);

// Uses OpenGL to transform an image using a matrix
void transform_image(struct image& img, const mat<3>& matrix, const struct config& cfg);

#endif // PNGSQ_PERSPECTIVE_HPP
