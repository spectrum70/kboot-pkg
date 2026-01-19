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
#include <Library/DevicePathLib.h>
#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/SimpleFileSystem.h>

#include "fs.h"
#include "utils.h"
#include "log.h"

#define MBR_LINUX_NATIVE 0x83

VOID EFIAPI fs_sort_details_by_creation(struct fs_file_details *entries)
{
	int n, i, j;

	/* Count items first. */
	n = 0;
	while (entries[n].device_handle != 0)
		n++;

	for (i = 0; i < n - 1; i++) {
		// Last i elements are already in place
         	for (j = 0; j < n - i - 1; j++) {
          		if (utils_compare_efi_time(&entries[j].creation_time,
                            &entries[j + 1].creation_time) > 0) {
				// Swap arr[j] and arr[j+1]
				struct fs_file_details temp = entries[j];
				entries[j] = entries[j + 1];
				entries[j + 1] = temp;
            		}
          	}
	}
}

EFI_GUID gLinuxRootPartitionGuid = { 0x4F68BCE3, 0xE8CD, 0x4DB1, \
	{ 0x96, 0xE7, 0xFB, 0xCA, 0xF9, 0x84, 0xB7, 0x09 } };

EFI_STATUS EFIAPI fs_find_boot_entry(IN EFI_HANDLE *handle_root,
				     IN CHAR16 *path,
				     IN EFI_FILE_PROTOCOL *boot_dir,
	                IN struct fs_file_details entries[MAX_BOOT_ENTRIES])
{
	EFI_FILE_INFO *f_info;
	EFI_STATUS status;
	BOOLEAN no_more_files = FALSE;
	UINTN entry = 0;

	/* get fisrt free slot */
	while(entries[entry].device_handle != 0) {
		if (entry == MAX_BOOT_ENTRIES - 1)
			return EFI_NOT_FOUND;
		entry++;
	}

	status = FileHandleFindFirstFile(boot_dir, &f_info);
	while (!EFI_ERROR(status) && !no_more_files) {
      		// Process the entry (e.g., check for vmlinuz)
        	if (StrnCmp(f_info->FileName, L"vmlinuz", 7) == 0) {
			struct fs_file_details ffd;

			SetMem(&ffd, sizeof(struct fs_file_details), 0);

			ffd.device_handle = handle_root;
			ffd.creation_time = f_info->CreateTime;

			StrCatS(ffd.path_name, MAX_PATH_NAME, path);
			StrCatS(ffd.path_name, MAX_PATH_NAME, L"\\");
			StrCatS(ffd.path_name, MAX_PATH_NAME, f_info->FileName);

             		entries[entry++] = ffd;
         	}
           	status = FileHandleFindNextFile(boot_dir, f_info,
            					&no_more_files);
	}

        return EFI_NOT_FOUND;
}

EFI_STATUS fs_look_for_linux_images(IN EFI_HANDLE *handle_root,
	               IN struct fs_file_details entries[MAX_BOOT_ENTRIES])
{
	EFI_STATUS status;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	EFI_FILE_PROTOCOL *root, *boot_dir;

	status = gBS->HandleProtocol(handle_root,
				     &gEfiSimpleFileSystemProtocolGuid,
				     (VOID**)&fs);
	if (EFI_ERROR(status)) {
		err(L"fs: cannot open siple fs protocol");
		return status;
	}

	/*
	 * All partitions in a disk can be 0x83, we cannot rely
	 * on that fs-type flag only,
	 */
	status = fs->OpenVolume(fs, &root);
	if (EFI_ERROR(status)) {
		err(L"fs: cannot open root volume");
		return status;
	}

	/* 1. We can be in EFI partition, look for images in / */
	status = root->Open(root, &boot_dir, L"\\", EFI_FILE_MODE_READ, 0);
	if (!EFI_ERROR(status)) {
		return fs_find_boot_entry(handle_root, L"\\", boot_dir, entries);
	} else {
		/* 2. We can be in ext4 partition, look for images in /boot */
		status = root->Open(root, &boot_dir,
				    L"boot", EFI_FILE_MODE_READ, 0);
		if (!EFI_ERROR(status))
			return fs_find_boot_entry(
				handle_root, L"boot", boot_dir, entries);
	}

	return EFI_NOT_FOUND;
}

EFI_STATUS EFIAPI fs_find_mbr_linux_entries(
		IN struct fs_file_details entries[MAX_BOOT_ENTRIES])
{
	UINTN count;
	EFI_HANDLE *handles;
	EFI_STATUS status;
	EFI_PARTITION_INFO_PROTOCOL *part_info;

	status = gBS->LocateHandleBuffer(ByProtocol,
                    &gEfiSimpleFileSystemProtocolGuid, NULL, &count, &handles);
	if (EFI_ERROR(status))
		return status;

	for (UINTN i = 0, status = EFI_NOT_FOUND; i < count; i++) {
	        // 2. Get Partition Info to check if it's MBR or GPT
	        status = gBS->HandleProtocol(handles[i],
	                        &gEfiPartitionInfoProtocolGuid,
	                        (VOID**)&part_info);
	        if (EFI_ERROR(status))
	        	continue;

        	if (part_info->Type == PARTITION_TYPE_MBR) {
         		if (part_info->Info.Mbr.OSIndicator ==
           			MBR_LINUX_NATIVE) {
                  		status = fs_look_for_linux_images(handles[i],
                                                                  entries);
                  		if (status != EFI_SUCCESS)
                      			break;
              		}
         	}
    	}

	if (handles != NULL)
    		FreePool(handles);

	fs_sort_details_by_creation(entries);

	return status;
}

/* To be tested for gpt cases */
EFI_STATUS EFIAPI fs_find_gpt_linux_root(OUT EFI_HANDLE *root_handle)
{
	UINTN count;
	EFI_HANDLE *handles;
	EFI_STATUS status;

	status = gBS->LocateHandleBuffer(ByProtocol,
     					 &gEfiBlockIoProtocolGuid,
        				 NULL, &count, &handles);
	if (EFI_ERROR(status) || count == 0)
      		return status;

	status = EFI_NOT_FOUND;

	for (UINTN i = 0, *root_handle = NULL; i < count; i++) {
		EFI_DEVICE_PATH_PROTOCOL *dpath;

         	dpath = DevicePathFromHandle(handles[i]);

          	while (!IsDevicePathEnd(dpath)) {
           		if (DevicePathType(dpath) == MEDIA_DEVICE_PATH &&
                            DevicePathSubType(dpath) == MEDIA_HARDDRIVE_DP) {
                            	HARDDRIVE_DEVICE_PATH *hd;

                        	hd = (HARDDRIVE_DEVICE_PATH *)dpath;

                         	if (CompareGuid((EFI_GUID *)&(hd->Signature),
                                         	&gLinuxRootPartitionGuid)) {
                        		*root_handle = (UINTN)handles[i];
                          		return EFI_SUCCESS;
                                }
                        }
             		dpath = NextDevicePathNode(dpath);
           	}

            	if (*(EFI_HANDLE *)root_handle != NULL)
             		break;
        }

        return status;
}

EFI_STATUS fs_get_boot_entries(IN EFI_HANDLE efi_handle,
		IN struct fs_file_details entries[MAX_BOOT_ENTRIES])
{
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
	EFI_STATUS status;

	status = gBS->HandleProtocol(efi_handle, &gEfiLoadedImageProtocolGuid,
				     (VOID **)&loaded_image);
	if (EFI_ERROR(status)) {
		err(L"fs: cannot handle loaded image protocol");
		return status;
	}

	status = fs_find_mbr_linux_entries(entries);
	if (EFI_ERROR(status)) {
		err(L"fs: cannot find any root device, exiting");
		return status;
	}
	/* TO DO, try gpt */

	return EFI_SUCCESS;
}

EFI_STATUS fs_load_bmp(IN CHAR16 *name, OUT VOID **buffer, OUT UINTN *size)
{
 	EFI_STATUS  status;
  	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
   	EFI_FILE_PROTOCOL *root, *file;
    	UINTN file_info_size = 0;
     	EFI_FILE_INFO *file_info = NULL;

      	status = gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid,
     				     NULL, (VOID**)&fs);
     	if (EFI_ERROR(status))
     		return status;

      	status = fs->OpenVolume(fs, &root);
       	status = root->Open(root, &file, name, EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(status))
        	return status;

        status = file->GetInfo(file, &gEfiFileInfoGuid, &file_info_size, NULL);
        if (status == EFI_BUFFER_TOO_SMALL) {
        	file_info = AllocatePool(file_info_size);
         	file->GetInfo(file, &gEfiFileInfoGuid, &file_info_size, file_info);
          	*size = (UINTN)file_info->FileSize;
           	*buffer = AllocatePool(*size);
         	status = file->Read(file, size, *buffer);
        }

        file->Close(file);
        root->Close(root);

        return status;
 }
