#ifndef fs_h
#define fs_h

#define MAX_BOOT_ENTRIES	32
#define MAX_PATH_NAME		256

struct fs_file_details {
	EFI_HANDLE device_handle;
	EFI_TIME creation_time;
	CHAR16 path_name[MAX_PATH_NAME];
};

EFI_STATUS fs_get_boot_entries(IN EFI_HANDLE img_handle,
		IN struct fs_file_details entries[MAX_BOOT_ENTRIES]);
EFI_STATUS fs_load_bmp(IN CHAR16 *name, OUT VOID **buffer, OUT UINTN *size);

#endif /* fs_h */
