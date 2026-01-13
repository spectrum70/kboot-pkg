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
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>

#include "loader.h"
#include "log.h"
#include "menu.h"

EFI_STATUS EFIAPI uefi_main(IN EFI_HANDLE efi_handle,
	                    IN EFI_SYSTEM_TABLE *sys_table)
{
	EFI_STATUS status;

	gST->ConOut->ClearScreen(gST->ConOut);

	log(L"loading drivers ...");

	status = loader_load_driver(efi_handle,
		                    L"\\EFI\\kboot\\drivers\\ext4_x64.efi");
	if (EFI_ERROR(status)) {
		err(L"error loading driver");
		return status;
	}

	return menu_exec(efi_handle);
}
