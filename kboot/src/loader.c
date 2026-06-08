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
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/LoadFile2.h>
#include <Protocol/SimpleFileSystem.h>

#include <Guid/FileInfo.h>

#include "log.h"

#define MAX_CMD_LINE		2048
#define MAX_UUID_LEN		64

#define EXT4_SUPERBLOCK_OFFSET	1024
#define EXT4_UUID_OFFSET	104
#define EXT4_MAGIC_NUMBER	0xef53
#define EXT4_MAGIC_OFFSET	56

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

EFI_STATUS loader_load_cmdline_opts(CHAR16 *cmdline)
{
	/* Load options string */

	return EFI_SUCCESS;
}

EFI_STATUS loader_load_rootfs_uuid(IN EFI_HANDLE part_handle, CHAR16 *uuid)
{
	EFI_STATUS status;
	EFI_DISK_IO_PROTOCOL *disk_io;
	EFI_BLOCK_IO_PROTOCOL *block_io;
	UINT8 tmp_uuid[32] = {0};
	UINT16 magic;
	CHAR16 *ptr;

	status = gBS->HandleProtocol(part_handle, &gEfiDiskIoProtocolGuid,
				     (VOID **)&disk_io);
	if (EFI_ERROR(status)) {
		err(L"\ndisk io protocol: %r", status);
		return status;
	}

	status = gBS->HandleProtocol(part_handle, &gEfiBlockIoProtocolGuid,
				     (VOID **)&block_io);
	if (EFI_ERROR(status)) {
		err(L"\nblock io protocol: %r", status);
		return status;
	}

	/* Check for EXT4 */
	status = disk_io->ReadDisk(disk_io, block_io->Media->MediaId,
				   (UINT64)
				   (EXT4_SUPERBLOCK_OFFSET + EXT4_MAGIC_OFFSET),
				   sizeof(UINT16), &magic);
	if (EFI_ERROR(status)) {
		err(L"\ngetting uuid: %r", status);
		return status;
	}

	if (magic != EXT4_MAGIC_NUMBER)
		return EFI_UNSUPPORTED;

	status = disk_io->ReadDisk(disk_io, block_io->Media->MediaId,
				   (UINT64)
				   (EXT4_SUPERBLOCK_OFFSET + EXT4_UUID_OFFSET),
				   16, tmp_uuid);
	if (EFI_ERROR(status)) {
		err(L"\ngetting uuid: %r", status);
		return status;
	}

	UnicodeSPrint(uuid, MAX_UUID_LEN * sizeof(CHAR16), 
		      L"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
		       "%02x%02x%02x%02x%02x%02x",
		      tmp_uuid[0], tmp_uuid[1], tmp_uuid[2], tmp_uuid[3],
		      tmp_uuid[4], tmp_uuid[5],
		      tmp_uuid[6], tmp_uuid[7],
		      tmp_uuid[8], tmp_uuid[9],
		      tmp_uuid[10], tmp_uuid[11], tmp_uuid[12],
		      tmp_uuid[13], tmp_uuid[14], tmp_uuid[15]);

	ptr = uuid;
	while (*ptr) {
		if (*ptr >= L'A' && *ptr <= L'F')
			*ptr += (L'a' - L'A');
		ptr++;
	}

	return EFI_SUCCESS;
}

EFI_STATUS loader_find_rootfs_uuid(IN EFI_HANDLE efi_handle, CHAR16 *uuid)
{
	EFI_STATUS status;
	EFI_HANDLE *handle_buffer;
	EFI_DEVICE_PATH_PROTOCOL *device_path;
	UINTN handle_count;

	status = gBS->LocateHandleBuffer(ByProtocol,
					 &gEfiBlockIoProtocolGuid,
					 NULL,
					 &handle_count,
					 &handle_buffer);
	if (EFI_ERROR(status)) {
		err(L"\nlocating handle buffer: %r", status);
		return status;
	}

	status = EFI_NOT_FOUND;

	for (UINTN i = 0; i < handle_count; ++i) {
		device_path = DevicePathFromHandle(handle_buffer[i]);
		if (device_path == NULL)
			continue;
		while (!IsDevicePathEnd(device_path)) {
			if ((DevicePathType(device_path) ==
				MEDIA_DEVICE_PATH) &&
			    (DevicePathSubType(device_path) ==
				MEDIA_HARDDRIVE_DP)) {

				status = loader_load_rootfs_uuid(
					handle_buffer[i], uuid);
				if (!EFI_ERROR(status))
					goto exit_found;
			}
			device_path = NextDevicePathNode(device_path);
		}
	}

exit_found:
	FreePool(handle_buffer);

	return status;
}


EFI_STATUS EFIAPI loader_load_linux_kernel(IN EFI_HANDLE efi_handle,
					   IN CHAR16 *img_file_path)
{
	EFI_STATUS status;
	EFI_HANDLE handle_kernel;
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image, *kernel_loaded_image;
	EFI_DEVICE_PATH_PROTOCOL *file_path;
	CHAR16 cmd_line[MAX_CMD_LINE];
	CHAR16 uuid[MAX_UUID_LEN] = {0};

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

	status = loader_find_rootfs_uuid(efi_handle, uuid);
	if (EFI_ERROR(status)) {
		err(L"\nloader_find_rootfs_uuid failed to fined ext4 uuid %r",
		    status);
	}

	/* CMDLINE composition now */
	StrCpyS(cmd_line, MAX_CMD_LINE, L"root=UUID=");
	StrCatS(cmd_line, MAX_CMD_LINE, uuid);

	/* TODO: read from file */
	StrCatS(cmd_line, MAX_CMD_LINE, L" rw nouveau.debug=info nouveau.config=NvGspRm=1 nouveau.runpm=0 init=/usr/local/bin/sysghost initrd=initramfs");

	/* Connecting remaining name of initramfs, come from vmlinux[8] */
	StrCatS(cmd_line, MAX_CMD_LINE, &img_file_path[8]);
	StrCatS(cmd_line, MAX_CMD_LINE, L".img");

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
