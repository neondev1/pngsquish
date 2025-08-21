#include <algorithm>
#include <cstdint>
#include <cstring>

#include "buffer.hpp"

size_t buffer::put(char const* data, size_t count) {
	if (!this->buf)
		return 0;
	const size_t bytes = std::min(this->len - this->pos, count);
	std::memcpy(this->buf + this->pos, data, bytes);
	this->pos += bytes;
	return bytes;
}

size_t buffer::put(char ch) {
	if (!this->buf || this->pos == this->len)
		return 0;
	buf[this->pos++] = ch;
	return 1;
}

size_t buffer::put(uint32_t val) {
	size_t bytes;
	for (bytes = 0; bytes < 4; bytes++) {
		if (this->put((char)(val >> 24)) == 0)
			return bytes;
		val <<= 8;
	}
	return bytes;
}

void buffer::alloc(size_t count) {
	char* nbuf = new char[count];
	if (this->buf) {
		std::memcpy(nbuf, this->buf, std::min(this->len, count));
		this->pos = std::min(this->pos, count);
	}
	else
		this->pos = 0;
	delete[] this->buf;
	this->buf = nbuf;
	this->len = count;
}

void buffer::free(void) {
	delete[] this->buf;
	this->buf = nullptr;
	this->len = 0;
	this->pos = 0;
}
