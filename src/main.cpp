#include <iostream>
#include <string>

#include "head.hpp"

int main(void) {
	libdeflate_compressor* compressor = libdeflate_alloc_compressor(12);
	if (!compressor)
		return EXIT_FAILURE;
	img_t image{};
	do {
		std::cout << "Path to image file: ";
		std::string path;
		std::getline(std::cin, path);
		image = load_image(path.c_str());
	} while (!image.data);
	std::cout << "Output path: ";
	std::string path;
	std::getline(std::cin, path);
	cfg_t config = { 10.0, 0.2, 0.2, 0.2, 0.1, true, 0.2, false, true, {255, 255, 255}, 1048576, 32 };
	preprocess_image(image, config);
	process_image(image, config);
	write_image(image, path.c_str(), compressor);
	free_image(image);
	return EXIT_SUCCESS;
}
