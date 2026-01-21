
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

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/LoadFile2.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

#include "log.h"

VOID *initrd_data;
UINTN initrd_size;

EFI_STATUS EFIAPI loader_load_driver(EFI_HANDLE efi_handle, CHAR16 *driver_path)
{
	EFI_STATUS status;
	EFI_HANDLE driver;
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
	EFI_DEVICE_PATH_PROTOCOL *driver_path_p;

	status = gBS->HandleProtocol(efi_handle, &gEfiLoadedImageProtocolGuid,
				     (VOID **)&loaded_image);
	if (EFI_ERROR(status)) {
		err(L"loading driver: handle image protocol: %r", status);
		return status;
	}

	driver_path_p = FileDevicePath(loaded_image->DeviceHandle, driver_path);

	status = gBS->LoadImage(FALSE, efi_handle, driver_path_p, NULL, 0,
				&driver);

	if (EFI_ERROR(status)) {
		err(L"error loading driver image");
		return status;
	}

	status = gBS->StartImage(driver, NULL, NULL);
	if (EFI_ERROR(status)) {
		err(L"cannot start image");
		return status;
	}

	gBS->ConnectController(gImageHandle, NULL, NULL, TRUE);

	FreePool(driver_path_p);

	return status;
}

EFI_STATUS loader_load_initramfs(IN CHAR16 *file_name)
{
	EFI_STATUS status;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	EFI_FILE_PROTOCOL *root, *file;
	EFI_FILE_INFO *f_info;
	UINTN info_size = 0;

	status = gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL,
				     (VOID**)&fs);
	if (EFI_ERROR(status))
		return status;

	fs->OpenVolume(fs, &root);
	status = root->Open(root, &file, file_name, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(status))
		return status;

	file->GetInfo(file, &gEfiFileInfoGuid, &info_size, NULL);
	f_info = AllocatePool(info_size);
	file->GetInfo(file, &gEfiFileInfoGuid, &info_size, f_info);
	initrd_size = (UINTN)f_info->FileSize;
	FreePool(f_info);

	initrd_data = AllocatePool(initrd_size);
	status = file->Read(file, &initrd_size, initrd_data);

	file->Close(file);
	root->Close(root);

	dbg(__func__, L"all ok, initrd_size %d", initrd_size);

	return status;
}

EFI_STATUS EFIAPI loader_load_linux_kernel(IN EFI_HANDLE efi_handle,
					   IN CHAR16 *img_file_path)
{
	EFI_STATUS status;
	EFI_HANDLE handle_kernel;
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image, *kernel_loaded_image;
	EFI_DEVICE_PATH_PROTOCOL *file_path;

	status = gBS->HandleProtocol(efi_handle, &gEfiLoadedImageProtocolGuid,
				    (VOID **)&loaded_image);
	if (EFI_ERROR(status)) {
		err(L"loading kernel: handle image protocol: %r", status);
		return status;
	}

	/*
	 * Strip away the // root, not welcome.
	 * TODO: strip it away only when images are in the efi root.
	 */
	if (*img_file_path == '\\')
		img_file_path++;
	file_path = FileDevicePath(loaded_image->DeviceHandle, img_file_path);
	if (file_path == NULL) {
		err(L"wrong path decoding, %s", img_file_path);
		return EFI_NOT_FOUND;
	}

	/* Note, this works only for efi files */
	status = gBS->LoadImage(FALSE, efi_handle,
	file_path, NULL, 0, &handle_kernel);
	if (EFI_ERROR(status)) {
		err(L"load image [%s] failed: %r", img_file_path, status);
		return status;
	}

	CHAR16 cmd_line[1024] = L"root=UUID=f946efca-0772-4336-a76c-eaa3e2b4f6d0 rw nouveau.debug=info nouveau.config=NvGspRm=1 nouveau.runpm=0 init=/usr/local/bin/sysghost initrd=initramfs";
	StrCatS(cmd_line, 1024, &img_file_path[8]);
	StrCatS(cmd_line, 1024, L".img");

	status = gBS->HandleProtocol(handle_kernel,
		    &gEfiLoadedImageProtocolGuid, (VOID**)&kernel_loaded_image);

	if (!EFI_ERROR(status)) {
		kernel_loaded_image->LoadOptionsSize =
			(UINT32)StrSize(cmd_line);
		kernel_loaded_image->LoadOptions =
			AllocateCopyPool(kernel_loaded_image->LoadOptionsSize,
					 cmd_line);
	} else {
		err(L"loading kernel: handle image protocol for kernel image: "
			"%r", status);
		return status;
	}

	status = gBS->StartImage(handle_kernel, NULL, NULL);

	return status;
}
