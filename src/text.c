#include "../inc/text.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

void pixel_set(uint32_t x, uint32_t y, uint32_t* buffer, uint32_t color);

// stolen from raylib, but only a single global buffer
#define MAX_TEXT_BUFFER_LENGTH 1024
const char* string_format(const char* string, ...) {

	static char buffer[MAX_TEXT_BUFFER_LENGTH];

	memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);   // Clear buffer before using

	va_list args;
	va_start(args, string);
	int requiredByteCount = vsnprintf(buffer, MAX_TEXT_BUFFER_LENGTH, string, args);
	va_end(args);

	// If requiredByteCount is larger than the MAX_TEXT_BUFFER_LENGTH, then overflow occured
	if (requiredByteCount >= MAX_TEXT_BUFFER_LENGTH) {
	// Inserting "..." at the end of the string to mark as truncated
		char* truncBuffer = buffer + MAX_TEXT_BUFFER_LENGTH - 4; // Adding 4 bytes = "...\0"
		sprintf(truncBuffer, "...");
	}

	return buffer;
}

void char_render(char c, uint32_t x, uint32_t y, uint32_t* buffer, Color color, uint32_t scale) {

	// convert char to index
	c &= 0x7F;
	if (c < ' ') {
		c = 0;
	} else {
		c -= ' ';
	}

	const uint8_t* chr = font[(unsigned char)c];

	for (size_t i = 0; i < CHAR_HEIGHT_FONT; i++) {
	for (size_t j = 0; j < CHAR_WIDTH_FONT ; j++) {
		if (chr[j] & (1<<i)) {
			for (size_t sy = 0; sy < scale; sy++) {
			for (size_t sx = 0; sx < scale; sx++) {
				pixel_set(x+j*scale+sx, y+i*scale+sy, buffer, color);
			}}
		}
	}}
}

void text_render(const char* str, uint32_t x, uint32_t y, uint32_t* buffer, Color color, uint32_t scale) {

	while (*str) {
		char_render(*str++, x, y, buffer, color, scale);
		x += CHAR_WIDTH_FONT*scale;
	}
}

