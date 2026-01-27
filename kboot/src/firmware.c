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

#include <Library/PrintLib.h>

#include <Library/DebugLib.h>
#include <Library/SmbiosLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/Smbios.h>

#include "log.h"

EFI_STATUS EFIAPI firmware_get_mb_info(OUT CHAR8 *fw_info)
{
	EFI_SMBIOS_PROTOCOL *smbios;
	EFI_SMBIOS_HANDLE handle = SMBIOS_HANDLE_PI_RESERVED;
	EFI_SMBIOS_TABLE_HEADER *record;
	EFI_STATUS status;
	CHAR8 *info_string;
	int n;

	gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL, (VOID**)&smbios);

	while ((status = smbios->GetNext(smbios, &handle, NULL, &record, NULL))
		== EFI_SUCCESS) {
		if (record->Type == 0) {
			UINT8 idx_name = *(UINT8*)((UINT8*)record + 0x04);
			UINT8 idx_ver = *(UINT8*)((UINT8*)record + 0x05);

			info_string = SmbiosLibReadString(record, idx_name);
			if (info_string != NULL) {
				n = AsciiSPrint(fw_info, 64, "%a ",
						info_string);
			}
			info_string = SmbiosLibReadString(record, idx_ver);
			if (info_string != NULL) {
				AsciiSPrint(&fw_info[n], 64, "%a",
					    info_string);
			}
			break;
		}
	}

	return status;
}
