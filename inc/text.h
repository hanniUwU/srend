#ifndef TEXT_H
#define TEXT_H

#include "../inc/5x5_font.h"
#include "../inc/color.h"

#include <stdint.h>
#include <stddef.h>

static const size_t CHAR_WIDTH_FONT  = 6;
static const size_t CHAR_HEIGHT_FONT = 8;

const char* string_format(const char* string, ...);
void char_render(char c, uint32_t x, uint32_t y, uint32_t* buffer, Color color, uint32_t scale);
void text_render(const char* str, uint32_t x, uint32_t y, uint32_t* buffer, Color color, uint32_t scale);

#endif

