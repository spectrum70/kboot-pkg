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

#include <Pi/PiFirmwareFile.h>

#include <Library/BmpSupportLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextOut.h>

#include "cpu.h"
#include "loader.h"
#include "log.h"
#include "memory.h"
#include "fs.h"
#include "utils.h"
#include "version.h"

#define MENU_START_ROW	15
#define MENU_X_COL	7

static int selected;

EFI_STATUS menu_draw_image(IN VOID *bmp_data, IN UINTN bmp_size)
{
	EFI_STATUS status;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *blt_buffer = NULL;
	UINTN blt_buffer_size = 0;
	UINTN height, width;

	status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,
                  		     NULL, (VOID **)&gop);
        if (EFI_ERROR(status))
        	return status;

        status = TranslateBmpToGopBlt(bmp_data, bmp_size, &blt_buffer,
                		      &blt_buffer_size, &height, &width);
        if (EFI_ERROR(status))
        	return status;

        status = gop->Blt(gop, blt_buffer, EfiBltBufferToVideo,
        		     0, 0, /* Source X, Y (Start of our buffer) */
                             42, 0, /* Destination X, Y (Top-left corner) */
                             width, height,
                             0     /* Delta (0 = buffer is tightly packed) */
                             );

        gBS->FreePool(blt_buffer);

        return status;
}

EFI_STATUS menu_load_bitmap(OUT VOID **bmp_data, OUT UINTN *bmp_size)
{
	return fs_load_bmp(L"\\EFI\\kboot\\images\\kboot.bmp", bmp_data, bmp_size);
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
			Print(L"] %s", name);
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

EFI_STATUS menu_exec(IN EFI_HANDLE img_handle)
{
	struct fs_file_details entries[MAX_BOOT_ENTRIES] = {0};
	EFI_INPUT_KEY key;
	UINTN i = 0, total_entries = 0;
	UINTN bmp_size, row;
	UINT64 total_memory;
	VOID *bmp_data;
	EFI_STATUS status;

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

	status = menu_draw_image(bmp_data, bmp_size);
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
