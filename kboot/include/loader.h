#ifndef loader_h
#define loader_h

EFI_STATUS EFIAPI loader_load_driver(EFI_HANDLE parent_img,
				     CHAR16 *driver_path);
EFI_STATUS EFIAPI loader_load_linux_kernel(IN EFI_HANDLE efi_handle,
					   IN CHAR16 *img_file_path);
#endif /* loader_h */
