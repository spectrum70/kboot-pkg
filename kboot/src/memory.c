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

#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

EFI_STATUS memory_get_total(OUT UINT64 *total_bytes)
{
	EFI_STATUS status;
	UINTN memory_map_size = 0;
	EFI_MEMORY_DESCRIPTOR *memory_map = NULL;
	UINTN map_key;
	UINTN descriptor_size;
	UINT32 descriptor_version;
	UINTN index;
	UINT64 total_pages = 0;

	status = gBS->GetMemoryMap(&memory_map_size, NULL,
			&map_key, &descriptor_size, &descriptor_version);

	memory_map_size += 2 * descriptor_size;
	memory_map = AllocatePool(memory_map_size);

	status = gBS->GetMemoryMap(&memory_map_size,
		memory_map, &map_key, &descriptor_size, &descriptor_version);

	if (EFI_ERROR(status))
		return status;

	for (index = 0; index < (memory_map_size / descriptor_size); index++) {
		EFI_MEMORY_DESCRIPTOR *desc =
			(EFI_MEMORY_DESCRIPTOR *)((UINT8 *)memory_map +
				(index * descriptor_size));
     		if (desc->Type == EfiReservedMemoryType)
       			continue;
		total_pages += desc->NumberOfPages;
	}

	*total_bytes = total_pages * 4096;

	FreePool(memory_map);

	return EFI_SUCCESS;
}
