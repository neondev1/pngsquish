#include "buffer.hpp"

#include <algorithm>

size_t buffer::put(char const* data, size_t count) {
	if (!this->buf)
		return 0;
	const size_t bytes = std::min(this->len - this->pos, count);
	std::memcpy(this->buf + this->pos, data, bytes);
	this->pos += bytes;
	return bytes;
}

size_t buffer::put(char data) {
	if (!this->buf || this->pos == this->len)
		return 0;
	buf[this->pos++] = data;
	return 1;
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
	this->pos = 0;
}

void buffer::dealloc(void) {
	if (this->buf)
		delete[] this->buf;
	this->buf = nullptr;
	this->len = 0;
	this->pos = 0;
}
