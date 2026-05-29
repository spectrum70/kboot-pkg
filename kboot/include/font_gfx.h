#ifndef font_gfx_h
#define font_gfx_h

#include <stdint.h>

typedef struct {
	uint16_t bitmap_offs;	/* Index/pointer into the GFXfont->bitmap array */
	uint8_t width;		/* Bitmap width in pixels */
	uint8_t height;		/* Bitmap height in pixels */
	uint8_t x_advance;	/* Distance to advance the cursor on the X axis */
	int8_t x_offset;	/* distance from cursor position to the glyph's Top-Left corner */
	int8_t y_offset;	/* Y distance from cursor position to the glyph's Top-Left corner */
} gfx_glyph;

typedef struct {
	uint8_t* bitmap;	/* Pointer to the concatenated array of all glyph bitmaps */
	gfx_glyph* glyph;	/* Pointer to the array of GFXglyph structures */
	uint16_t first;		/* ASCII code of the first character in this font */
	int16_t last;		/* ASCII code of the last character in this font */
	uint8_t y_advance;	/* Newline distance (how much to move Y for the next line */
} gfx_font;

#endif /* font_gfx_h */
