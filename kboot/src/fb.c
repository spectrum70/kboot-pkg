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

#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>

#include "fb.h"

static UINT32 *fb;
static UINT32 scan_line;

#define diether_border_d(x) (((x & 0xff) + 0x15) | ((x & 0xff00) - 0x3000) | \
			     ((x & 0xff0000) - 0x300000))
#define diether_border_e(x) (((x & 0xff) + 0x1d) | ((x & 0xff00) - 0x7000) | \
			     ((x & 0xff0000) - 0x700000))

static inline VOID fb_put_pixel(UINTN x, UINTN y, UINT32 color)
{
	fb[y * scan_line + x] = color;
}

static inline VOID fb_put_pixel_with_dithering(UINTN x, UINTN y, UINT32 color)
{
	fb[y * scan_line + x + 2] = diether_border_e(color);
	fb[y * scan_line + x + 1] = diether_border_d(color);
	fb[y * scan_line + x] = color;
	fb[y * scan_line + x - 1] = diether_border_d(color);
	fb[y * scan_line + x - 2] = diether_border_e(color);
	fb[(y + 2) * scan_line + x] = diether_border_e(color);
	fb[(y + 1) * scan_line + x] = diether_border_d(color);
	fb[(y - 1) * scan_line + x] = diether_border_d(color);
	fb[(y - 2) * scan_line + x] = diether_border_e(color);
}

static VOID fb_draw_circle_points(INTN x_center, INTN y_center,
				  INTN x, INTN y, UINT32 color)
{
	fb_put_pixel_with_dithering(x_center + x, y_center + y, color);
	fb_put_pixel_with_dithering(x_center - x, y_center + y, color);
	fb_put_pixel_with_dithering(x_center + x, y_center - y, color);
	fb_put_pixel_with_dithering(x_center - x, y_center - y, color);
	fb_put_pixel_with_dithering(x_center + y, y_center + x, color);
	fb_put_pixel_with_dithering(x_center - y, y_center + x, color);
	fb_put_pixel_with_dithering(x_center + y, y_center - x, color);
	fb_put_pixel_with_dithering(x_center - y, y_center - x, color);
}

/*
 * Params are x, y of the center, + radius
 */
VOID EFIAPI fb_draw_circle(UINTN x, UINTN y, UINTN radius, UINT32 color)
{
	INTN X = 0;
	INTN Y = radius;
	INTN D = 3 - 2 * radius; // Initial decision parameter

	fb_draw_circle_points(x, y, X, Y, color);

	while (Y >= X) {
		X++;
		if (D > 0) {
			Y--;
			D = D + 4 * (X - Y) + 10;
		} else {
			D = D + 4 * X + 6;
		}

		fb_draw_circle_points(x, y, X, Y, color);
	}
}

VOID EFIAPI fb_init(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop)
{
	fb = (UINT32 *)(UINTN)gop->Mode->FrameBufferBase;
	scan_line = gop->Mode->Info->PixelsPerScanLine;
}
