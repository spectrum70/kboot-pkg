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

#include <Library/BmpSupportLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Pi/PiFirmwareFile.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextOut.h>

#include "cpu.h"
#include "loader.h"
#include "log.h"
#include "memory.h"
#include "menu.h"
#include "fb.h"
#include "fs.h"
#include "utils.h"
#include "version.h"

#define MENU_START_ROW	15
#define MENU_X_COL	7

#define SEGMENTS	7
#define SEG_POINTS	6

#define PROGRESS_POS_X	260
#define PROGRESS_POS_Y  580
#define CHAR_0_OFF_X	(PROGRESS_POS_X - 22)
#define CHAR_0_OFF_Y    (PROGRESS_POS_Y - 46)

static int selected;

static struct point seg[SEGMENTS][SEG_POINTS] = { \
/* a */ {{11, 7}, {16, 0}, {126, 0}, {133, 7}, {105, 32}, {36, 32}},
/* b */ {{137, 13}, {143, 18}, {143, 106}, {127, 123}, {111, 106}, {111, 38}},
/* c */ {{138, 244}, {144, 239}, {144, 148}, {127, 133}, {112, 149}, {112, 218}}
/* d */ ,{{10, 248}, {35, 222}, {105, 222}, {132, 248}, {126, 255}, {16, 255}},
/* e */ {{16, 132}, {32, 148}, {32, 219}, {6, 244}, {0, 237}, {0, 148}},
/* f */ {{0, 18}, {0, 107}, {16, 122}, {32, 107}, {32, 36}, {7, 13}},
/* g */ {{122, 128}, {107, 112}, {35, 112}, {22, 128}, {35, 143}, {107, 143}},
};

void menu_draw_line(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
		    INTN x0, INTN y0, INTN x1, INTN y1,
	            EFI_GRAPHICS_OUTPUT_BLT_PIXEL color)
{
	INTN dx = ABS(x1 - x0), sx = x0 < x1 ? 1 : -1;
	INTN dy = -ABS(y1 - y0), sy = y0 < y1 ? 1 : -1;
	INTN err = dx + dy, e2;

	while (TRUE) {
		// Write pixel to GOP FrameBuffer at (x0, y0)
		UINT32 *FB = (UINT32 *)gop->Mode->FrameBufferBase;
		FB[y0 * gop->Mode->Info->PixelsPerScanLine + x0] =
			*(UINT32*)&color;

		if (x0 == x1 && y0 == y1) break;
		e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

EFI_STATUS menu_draw_segment(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
			     IN struct point *p,
			     IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL color)
{
	EFI_STATUS status = EFI_SUCCESS;
	UINTN pt, scale = 3;

 	for (pt = 0; pt < SEG_POINTS - 1; pt++) {
		menu_draw_line(gop,
		                  p[pt].x / scale + CHAR_0_OFF_X,
			          p[pt].y / scale + CHAR_0_OFF_Y,
		                  p[pt + 1].x / scale + CHAR_0_OFF_X,
				  p[pt + 1].y / scale + CHAR_0_OFF_Y,
		                  color);
   	}

  	menu_draw_line(gop,
   		       p[SEG_POINTS - 1].x / scale + CHAR_0_OFF_X,
                       p[SEG_POINTS - 1].y / scale + CHAR_0_OFF_Y,
		       p[0].x / scale + CHAR_0_OFF_X,
		       p[0].y / scale + CHAR_0_OFF_Y,
		       color);

   	return status;
}

EFI_STATUS menu_draw_image(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
			   IN VOID *bmp_data, IN UINTN bmp_size)
{
	EFI_STATUS status;
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *blt_buffer = NULL;
	UINTN blt_buffer_size = 0;
	UINTN height, width;

        status = TranslateBmpToGopBlt(bmp_data, bmp_size, &blt_buffer,
                		      &blt_buffer_size, &height, &width);
        if (EFI_ERROR(status))
        	return status;

        status = gop->Blt(gop, blt_buffer, EfiBltBufferToVideo,
        		  0, 0, 42, 0, width, height, 0);

        gBS->FreePool(blt_buffer);

        return status;
}

static CHAR8 lcd_num[10][SEGMENTS] = {
	/* a, b, c, d, e, f, g */
	{1, 1, 1, 1, 1, 1, 0},
	{0, 1, 1, 0, 0, 0, 0},
	{1, 1, 0, 1, 1, 0, 1},
	{1, 1, 1, 1, 0, 0, 1},
	{0, 1, 1, 0, 0, 1, 1},
	{1, 0, 1, 1, 0, 1, 1},
	{1, 0, 1, 1, 1, 1, 1},
	{1, 1, 1, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 0, 1, 1},
};

EFI_STATUS menu_show_lcd_num(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, UINTN n)
{
	UINTN i;
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL orange = {0, 255, 165, 0};
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL dark = {0, 20, 20, 20};

	for (i = 0; i < 7; ++i)
		menu_draw_segment(gop, seg[i], lcd_num[n][i] ? orange : dark);

	return EFI_SUCCESS;
}

EFI_STATUS menu_load_bitmap(OUT VOID **bmp_data, OUT UINTN *bmp_size)
{
	return fs_load_bmp(L"\\EFI\\kboot\\images\\kboot.bmp",
			   bmp_data, bmp_size);
}

EFI_STATUS menu_get_name(IN CHAR16 *path_name, OUT CHAR16 **ptr)
{
	UINTN len;

	if (path_name == NULL)
  		return EFI_INVALID_PARAMETER;

	len = StrLen(path_name);
	if (len == 0)
  		return EFI_INVALID_PARAMETER;

	*ptr = path_name + len - 1;
	while (*ptr >= path_name && **ptr != L'\\')
		(*ptr)--;

	(*ptr)++;

	return EFI_SUCCESS;
}

VOID menu_entries_display(struct fs_file_details entries[MAX_BOOT_ENTRIES], IN UINTN row)
{
	UINTN i = 0;
  	EFI_STATUS status;

	while (entries[i].device_handle) {
		CHAR16 *name;

		gST->ConOut->SetCursorPosition(gST->ConOut, MENU_X_COL, ++row);
		if (i == selected)
			gST->ConOut->SetAttribute(gST->ConOut,
			    EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));
		else
			gST->ConOut->SetAttribute(gST->ConOut,
			    EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));

		status = menu_get_name(entries[i].path_name, &name);
		if (!EFI_ERROR(status)) {
			Print(L"[");
			if (i == selected)
				gST->ConOut->SetAttribute(gST->ConOut,
				    EFI_TEXT_ATTR(EFI_BLACK, EFI_BLUE));
			Print(L" ");
			if (i == selected)
				gST->ConOut->SetAttribute(gST->ConOut,
				    EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTGRAY));
			Print(L"] %04d-%02d-%02d %02d:%02d:%02d",
				entries[i].creation_time.Year,
				entries[i].creation_time.Month,
				entries[i].creation_time.Day,
				entries[i].creation_time.Hour,
				entries[i].creation_time.Minute,
				entries[i].creation_time.Second
			);
			Print(L"  %s", name);
		}

		i++;
	}
	Print(L"\n");

	gST->ConOut->EnableCursor(gST->ConOut, FALSE);
}

EFI_STATUS menu_read_key(OUT EFI_INPUT_KEY *key)
{
	UINTN idx;

	gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &idx);

	return gST->ConIn->ReadKeyStroke(gST->ConIn, key);
}

static int countdown = 9;

static VOID menu_update_progress(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
				 UINT32 x, UINT32 y)
{
	//int i;
	//int dark = countdown * 10;

	fb_draw_circle(x, y, 100, 0x00ffff00);

	/*for (i = 100; i > dark; --i)
		fb_draw_circle(x, y, i, 0x000078B7);
	for (i = dark; i >= 0; --i)
		fb_draw_circle(x, y, i, 0x000018E7);*/

	menu_show_lcd_num(gop, countdown);
}

VOID EFIAPI menu_time_sec_callback(IN EFI_EVENT event, IN VOID *context)
{
	IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop =
			(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *)context;

	menu_update_progress(gop, PROGRESS_POS_X, PROGRESS_POS_Y);

	if (countdown-- == 0) {
		gBS->SetTimer(event, TimerCancel, 0);
	}
}

EFI_STATUS menu_exec(IN EFI_HANDLE img_handle)
{
	struct fs_file_details entries[MAX_BOOT_ENTRIES] = {0};
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_INPUT_KEY key;
	UINTN i = 0, total_entries = 0;
	UINTN bmp_size, row;
	UINT64 total_memory;
	VOID *bmp_data;
	EFI_EVENT periodic_event;
	EFI_STATUS status;

	status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,
                  		     NULL, (VOID **)&gop);
        if (EFI_ERROR(status))
        	return status;

        fb_init(gop);

	gST->ConOut->ClearScreen(gST->ConOut);

	status = fs_get_boot_entries(img_handle, entries);
	if (EFI_ERROR(status)) {
		err(L"getting boot entries\n");
		return status;
	}

	status = menu_load_bitmap(&bmp_data, &bmp_size);
	if (EFI_ERROR(status)) {
		err(L"bitmap not found, status = %d", status);
	}

	status = menu_draw_image(gop, bmp_data, bmp_size);
	if (EFI_ERROR(status)) {
		err(L"cannot display bitmap, status = %d", status);
	}

	while (entries[i++].device_handle != 0)
		total_entries++;

	memory_get_total(&total_memory);

	row = MENU_START_ROW;
	/* One line sep after title. */
	gST->ConOut->SetCursorPosition(gST->ConOut, MENU_X_COL, row++);

	gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
	Print(L"kboot v." version " - (C) 2026, Kernelspace\n");

	gST->ConOut->SetCursorPosition(gST->ConOut, MENU_X_COL, ++row);
	cpu_print_cpu_id();

	gST->ConOut->SetCursorPosition(gST->ConOut, MENU_X_COL, ++row);
	Print(L"Total memory: ");
	memory_get_total(&total_memory);
	utils_print_size(total_memory, P_MEGA);

	status = gBS->CreateEvent(EVT_TIMER | EVT_NOTIFY_SIGNAL,
                	TPL_CALLBACK, menu_time_sec_callback,
                 	gop, &periodic_event);

	if (!EFI_ERROR(status)) {
		status = gBS->SetTimer(periodic_event, TimerPeriodic, 10000000);
	}

	/* One line sep from info. */
	row++;

	for(;;) {
		menu_entries_display(entries, row);
		menu_read_key(&key);
		switch (key.ScanCode) {
		case 1:
	 		if (selected)
				selected--;
			break;
		case 2:
			if (selected < (total_entries - 1))
				selected++;
			break;
		case 0:
			status = loader_load_linux_kernel(
				img_handle,
				entries[selected].path_name);
			if (EFI_ERROR(status)) {
				err(L"cannot boot image %s",
				    entries[selected].path_name);
				for(;;);
			}
			break;
		default:
			break;
		}
	}
}
