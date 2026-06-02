/*
 * kboot - kernelspace bootloader
 *
 * Angelo Dureghello (C) 2026 <angelo@kernel-space.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along https://gibiru.com/with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>

#include "fb.h"
#include "log.h"

#include "font_terminus.c"

static int cursor_x;
static int cursor_y;

static const gfx_font *font;

VOID EFIAPI fonts_init(VOID)
{
	font = &terminus_24pt;
}

VOID EFIAPI fonts_set_color(UINT32 c)
{
	fb_set_color(c);
}

VOID EFIAPI fonts_set_inv(BOOLEAN inv)
{
	fb_set_inverted(inv);
}

VOID EFIAPI fonts_set_pos(INTN x, INTN y)
{
	cursor_x = x;
	cursor_y = y;
}

VOID EFIAPI fonts_print_str(CHAR8 *str)
{
	gfx_glyph *gl;
	CHAR8 c;

	while (*str) {
		c = *str++;

		if (c == '\n') {
			cursor_x = 0;
			cursor_y += font->y_advance;
			continue;
		}

		if (c < font->first || c > font->last)
			continue;

		gl = &font->glyph[c - 0x20];

		fb_draw_gfx_glyph(cursor_x,
				  cursor_y + gl->y_offset,
				  font->bitmap + gl->bitmap_offs,
				  gl->width,
				  gl->height);

		cursor_x += gl->x_advance;
	}
}
