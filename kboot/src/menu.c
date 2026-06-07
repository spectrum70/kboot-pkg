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
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Pi/PiFirmwareFile.h>
#include <Protocol/FirmwareManagement.h>
#include <Protocol/FormBrowser2.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiImage.h>
#include <Protocol/LockBox.h>
#include <Protocol/SimpleTextOut.h>

#include "cpu.h"
#include "fb.h"
#include "firmware.h"
#include "fs.h"
#include "fonts.h"
#include "loader.h"
#include "log.h"
#include "memory.h"
#include "menu.h"
#include "utils.h"
#include "version.h"

#include "lcd.c"

#define MAX_LCD_COLS	320

#define MENU_START_Y	650
#define MENU_X_COL	7

#define LCD_CHAR_ROWS	7
#define LCD_CHAR_COLS	5
#define LCD_PIXEL_SEP	1
#define LCD_CHAR_SEP	2
#define LCD_ROW_SEP	3
#define LCD_PIXEL_STEP	(FB_LCD_PIXEL_HW + LCD_PIXEL_SEP)
#define LCD_CHAR_NEXT	((LCD_PIXEL_STEP * LCD_CHAR_COLS) + LCD_CHAR_SEP)
#define LCD_ROW_NEXT	((LCD_PIXEL_STEP * LCD_CHAR_ROWS) + LCD_ROW_SEP)

#define PROGRESS_POS_X	42
#define PROGRESS_POS_Y  245

static int selected;
static int lcd_line_cols;
static int lcd_line_rows;
static int lcd_cursor_row;

static int countdown = 15;
static int prog_step_chars;

static EFI_LOCK p_lock;
static EFI_IMAGE_OUTPUT direct_screen;

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/HiiFont.h>

EFI_STATUS render_string_to_bitmap(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, CHAR16 *str)
{
	EFI_STATUS status;
	EFI_HII_FONT_PROTOCOL *hii_font;
	EFI_HII_OUT_FLAGS flags = EFI_HII_DIRECT_TO_SCREEN;
	EFI_IMAGE_OUTPUT *blt = &direct_screen; // NULL triggers auto-allocation

	status = gBS->LocateProtocol(&gEfiHiiFontProtocolGuid,
				     NULL, (VOID **)&hii_font);
	if (EFI_ERROR(status)) {
		log(L"ERROR");
		return status;
	}
	// Use default system font/colors by passing NULL for StringInfo
	status = hii_font->StringToImage(hii_font, flags, str, NULL, &blt,
					 0, 0, NULL, NULL, NULL);
	if (!EFI_ERROR(status)) {
		// Rendered data is in Blt->Image.Bitmap
		// Caller is responsible for freeing Blt and Blt->Image.Bitmap
	}
	return status;
}

VOID menu_set_osind_and_reset()
{
	UINT64 osind = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;

	gRT->SetVariable(L"OsIndications", &gEfiGlobalVariableGuid,
		EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
		EFI_VARIABLE_RUNTIME_ACCESS,
		sizeof(osind), &osind);

	gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
}

EFI_STATUS menu_osind_supported(VOID)
{
	UINT64 osind_supported;
	UINTN size = sizeof(osind_supported);
	EFI_STATUS status;

	status = gRT->GetVariable(L"OsIndicationsSupported",
				  &gEfiGlobalVariableGuid,
				  NULL, &size, &osind_supported);

	if (EFI_ERROR(status) ||
		!(osind_supported & EFI_OS_INDICATIONS_BOOT_TO_FW_UI)) {
			return EFI_UNSUPPORTED;
	}

	return status;
}

EFI_STATUS menu_get_current_mode(OUT UINTN *columns, OUT UINTN *rows)
{
	EFI_STATUS status;
	INT32 current_mode;

	current_mode = gST->ConOut->Mode->Mode;

	status = gST->ConOut->QueryMode(gST->ConOut, current_mode,
					columns, rows);

	return status;
}

EFI_STATUS menu_get_pixel_size(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
			       OUT UINT32 *pixel_width,
			       OUT UINT32 *pixel_height)
{
	if (gop->Mode != NULL && gop->Mode->Info != NULL) {
		*pixel_width  = gop->Mode->Info->HorizontalResolution;
		*pixel_height = gop->Mode->Info->VerticalResolution;
		return EFI_SUCCESS;
	}

	return EFI_NOT_FOUND;
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

EFI_STATUS menu_show_lcd_char(UINTN xpos, UINTN ypos, UINTN c)
{
	UINTN i, x, y;
	UINT32 orange = 0x00ffa500;
	UINT32 dark = 0x00070707;

	for (i = 0, y = ypos; i < LCD_CHAR_ROWS; ++i, y += LCD_PIXEL_STEP) {
		x = xpos;
		fb_draw_lcd_pixel(x, y, lcd_n[c][i] & 16 ? orange : dark);
		x += LCD_PIXEL_STEP;
		fb_draw_lcd_pixel(x, y, lcd_n[c][i] & 8 ? orange : dark);
		x += LCD_PIXEL_STEP;
		fb_draw_lcd_pixel(x, y, lcd_n[c][i] & 4 ? orange : dark);
		x += LCD_PIXEL_STEP;
		fb_draw_lcd_pixel(x, y, lcd_n[c][i] & 2 ? orange : dark);
		x += LCD_PIXEL_STEP;
		fb_draw_lcd_pixel(x, y, lcd_n[c][i] & 1 ? orange : dark);
	}

	return EFI_SUCCESS;
}

EFI_STATUS menu_load_bitmap(OUT VOID **bmp_data, OUT UINTN *bmp_size)
{
	return fs_load_file(L"\\EFI\\kboot\\images\\kboot.bmp",
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

VOID EFIAPI menu_set_lcd_pos(UINTN col, UINTN row)
{
	lcd_cursor_row = row * LCD_ROW_NEXT;
}

VOID menu_write_lcd_line(UINTN x, UINTN y, CHAR8 *str)
{
	int i = 0;

	while (*str && i++ < lcd_line_cols) {
		menu_show_lcd_char(x, y, *str++);
		x += LCD_CHAR_NEXT;
	}
	while (i++ < lcd_line_cols) {
		menu_show_lcd_char(x, y, ' ');
		x += LCD_CHAR_NEXT;
	}
	lcd_cursor_row += LCD_ROW_NEXT;
}

VOID EFIAPI menu_print_line(IN CONST CHAR8 *msg, ...)
{
	CHAR8 line[MAX_LCD_COLS] = {0};
	VA_LIST list;

   	VA_START(list, msg);
	AsciiVSPrint(line, MAX_LCD_COLS, msg, list);
	VA_END(list);

	menu_write_lcd_line(0, lcd_cursor_row, line);
}

VOID menu_entries_display(struct fs_file_details entries[MAX_BOOT_ENTRIES])
{
	UINTN i = 0;
	EFI_STATUS status;
	static CHAR8 line[MAX_LCD_COLS];

	while (entries[i].device_handle) {
		CHAR16 *name;
		UINTN pos = MENU_START_Y + i * 42;

		fonts_set_pos(0, pos);

		status = menu_get_name(entries[i].path_name, &name);
		if (!EFI_ERROR(status)) {
			UINT32 r_color;

			r_color = (i == selected) ? 0x00808080 : 0;
			fb_draw_rect(direct_screen.Image.Screen, 0, pos - 30,
				     direct_screen.Width, pos + 8, r_color);
			if (i == selected)
			fb_set_bg_color(0x00808080);

			AsciiSPrint(line, MAX_LCD_COLS,
				" %04d-%02d-%02d %02d:%02d:%02d  %s",
				entries[i].creation_time.Year,
				entries[i].creation_time.Month,
				entries[i].creation_time.Day,
				entries[i].creation_time.Hour,
				entries[i].creation_time.Minute,
				entries[i].creation_time.Second,
				name
			);

			EfiAcquireLock(&p_lock);
			fonts_set_color((i == selected) ?
					0x00efef00 : 0x003f3f3f);
			fb_set_bg_color((i == selected) ? r_color : 0);

			fonts_print_str(line);
			EfiReleaseLock(&p_lock);

			fb_set_bg_color(0x00000000);
		}

		i++;
	}

	gST->ConOut->EnableCursor(gST->ConOut, FALSE);
}

VOID EFIAPI menu_print_status_top()
{
	EFI_TIME time;
	UINTN n = 0;
	static CHAR8 line[MAX_LCD_COLS] = {0};

	while (n < (lcd_line_cols - 19))
		AsciiSPrint(&line[n++], MAX_LCD_COLS, " ");

	gRT->GetTime(&time, 0);

	AsciiSPrint(&line[n], MAX_LCD_COLS, "%02d-%02d-%04d %02d:%02d:%02d",
		    time.Day,
		    time.Month,
		    time.Year,
		    time.Hour,
		    time.Minute,
		    time.Second);

	menu_write_lcd_line(0, 0, line);
}

VOID EFIAPI menu_print_progress()
{
	CHAR8 line[MAX_LCD_COLS] = {0};
	int i, q, x;

	AsciiSPrint(line, MAX_LCD_COLS, " %02d ", countdown);

	for (i = 4, q = 0; q < countdown; i += prog_step_chars, q++) {
		for (x = 0; x < prog_step_chars; x++)
			line[i + x] = CHAR_INV;
	}

	menu_print_line(line);
}

VOID EFIAPI menu_time_sec_callback(IN EFI_EVENT event, IN VOID *context)
{
	EfiAcquireLock(&p_lock);

	menu_set_lcd_pos(0, 0);
	menu_print_status_top();

	menu_set_lcd_pos(0, lcd_line_rows - 1);
	menu_print_progress();

	EfiReleaseLock(&p_lock);

	if (countdown > 0)
		countdown--;
}

EFI_STATUS menu_read_key(OUT EFI_INPUT_KEY *key)
{
	UINTN idx;

	gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &idx);

	return gST->ConIn->ReadKeyStroke(gST->ConIn, key);
}

VOID menu_print_logo(VOID)
{
	menu_print_line(" %c    %c              %c",
		31, 31, 31);
	menu_print_line(" %c %c%c %c              %c%c%c",
		31, 30, 29, 31, 31, 31, 29);
	menu_print_line(" %c%c%c  %c%c%c%c %c%c%c%c %c%c%c%c %c",
		31, 31, 29, 31, 31, 31, 27, 30, 31, 31, 27, 30, 31, 31, 27, 31);
	menu_print_line(" %c%c%c  %c  %c %c  %c %c  %c %c",
		31, 28, 27, 31, 31, 31, 31, 31, 31, 31);
	menu_print_line(" %c %c%c %c%c%c%c %c%c%c%c %c%c%c%c %c%c%c",
		31, 28, 27, 28, 31, 31, 29, 28, 31, 31,
		29, 28, 31, 31, 29, 28, 31, 29);
}

EFI_STATUS menu_setup_header(VOID)
{
	CHAR8 line[MAX_LCD_COLS];
	CHAR8 fw_info[64];
	UINT64 total_memory;
	EFI_STATUS status;

	menu_print_status_top();
	menu_print_logo();

	menu_print_line("");
	menu_print_line(" v.%a - (c) 2026, Kernelspace", version);

	fonts_set_pos(0, 360);

	cpu_get_cpu_id(line);
	fonts_print_str(" CPU: ");
	fonts_print_str(line);
	fonts_print_str("\n");

	status = memory_get_total(&total_memory);
	if (EFI_ERROR(status))
		return status;

	utils_print_size_buff(line, total_memory, P_MEGA);
	fonts_print_str(" Total memory: ");
	fonts_print_str(line);
	fonts_print_str("\n");

	status = firmware_get_mb_info(fw_info);
	if (EFI_ERROR(status))
		return status;
	fonts_print_str(" Firmware name/version: ");
	fonts_print_str(fw_info);
	fonts_print_str("\n\n");

	fonts_print_str(" Select image to boot ...\n\n");

	menu_set_lcd_pos(0, lcd_line_rows - 2);
	menu_print_line(" F1 Power off    %aF3 Reboot",
			menu_osind_supported() != EFI_UNSUPPORTED ?
			"F2 Bios Setup   " : "");

	return EFI_SUCCESS;
}

EFI_STATUS menu_exec(IN EFI_HANDLE img_handle)
{
	struct fs_file_details entries[MAX_BOOT_ENTRIES] = {0};
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_INPUT_KEY key;
	EFI_EVENT periodic_event;
	EFI_STATUS status;
	UINT32 p_width, p_height;
	UINTN i = 0, total_entries = 0;

	status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,
				     NULL, (VOID **)&gop);
	if (EFI_ERROR(status))
		return status;

	EfiInitializeLock(&p_lock, TPL_CALLBACK);

        direct_screen.Width = gop->Mode->Info->HorizontalResolution;
        direct_screen.Height = gop->Mode->Info->VerticalResolution;
        direct_screen.Image.Screen = gop;

	fb_init(gop);
	fonts_init();

	status = menu_get_pixel_size(gop, &p_width, &p_height);
	if (EFI_ERROR(status))
		return status;

	lcd_line_cols = p_width / LCD_CHAR_NEXT;
	lcd_line_rows = p_height / LCD_ROW_NEXT;
	prog_step_chars = (lcd_line_cols - 4) / countdown;

	gST->ConOut->ClearScreen(gST->ConOut);

	menu_setup_header();

	gST->ConOut->EnableCursor(gST->ConOut, FALSE);

	status = fs_get_boot_entries(img_handle, entries);
	if (EFI_ERROR(status)) {
		err(L"getting boot entries\n");
		return status;
	}

	while (entries[i++].device_handle != 0)
		total_entries++;

	status = gBS->CreateEvent(EVT_TIMER | EVT_NOTIFY_SIGNAL,
				  TPL_CALLBACK, menu_time_sec_callback,
				  gop, &periodic_event);
	if (!EFI_ERROR(status)) {
		status = gBS->SetTimer(periodic_event, TimerPeriodic, 10000000);
	}

	menu_print_progress();

	for(;;) {
		menu_entries_display(entries);

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
			fonts_set_pos(0, MENU_START_Y +
				      total_entries * 44 + 50);
			fonts_print_str("     Loading kernel ...");
			status = loader_load_linux_kernel(img_handle,
					entries[selected].path_name);
			if (EFI_ERROR(status)) {
				err(L"cannot boot image %s",
				    entries[selected].path_name);
				for(;;);
			}
			break;
		case SCAN_F1:
			gRT->ResetSystem(EfiResetShutdown,
					 EFI_SUCCESS, 0, NULL);
			break;
		case SCAN_F2:
			if (menu_osind_supported() != EFI_UNSUPPORTED)
				menu_set_osind_and_reset();
			break;
		case SCAN_F3:
			gRT->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
			break;
		default:
			break;
		}
	}
}
