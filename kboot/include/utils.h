#ifndef utils_h
#define utils_h

enum p_mode {
	P_MEGA,
};

VOID utils_print_size(IN UINT64 size, IN enum p_mode mode);
INTN EFIAPI utils_compare_efi_time(IN EFI_TIME *time1, IN EFI_TIME *time2);

#endif /* utils_h */
