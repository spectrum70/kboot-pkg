
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
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>

#include "log.h"

EFI_STATUS loader_load_driver(EFI_HANDLE efi_handle, CHAR16 *driver_path)
{
	EFI_STATUS status;
	EFI_HANDLE driver;
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
	EFI_DEVICE_PATH_PROTOCOL *driver_path_p;

	status = gBS->HandleProtocol(efi_handle, &gEfiLoadedImageProtocolGuid,
				     (VOID **)&loaded_image);
	if (EFI_ERROR(status)) {
		err(L"loader: error in HandleProtocol");
		return status;
	}

	driver_path_p = FileDevicePath(loaded_image->DeviceHandle, driver_path);

	status = gBS->LoadImage(FALSE, efi_handle, driver_path_p, NULL, 0,
				&driver);

	if (EFI_ERROR(status)) {
		err(L"error loading driver image");
     		return status;
	}

	// 2. Start the driver image
	// This executes the driver's EntryPoint and installs DriverBinding
	// protocols
	status = gBS->StartImage(driver, NULL, NULL);
	if (EFI_ERROR(status)) {
		err(L"cannot start image");
		return status;
	}

	// 3. Connect the driver to all available controllers, if any
	// This forces the driver to check for ext4 partitions on all disks
	gBS->ConnectController(gImageHandle, NULL, NULL, TRUE);

	FreePool(driver_path_p);

	return status;
 }
