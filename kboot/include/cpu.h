#ifndef cpu_h
#define cpu_h

EFI_STATUS EFIAPI cpu_get_cpu_id(OUT CHAR8 *str_brand);
EFI_STATUS EFIAPI cpu_print_amd_ryzen_cpu_temp(VOID);

#endif /* cpu_h */
