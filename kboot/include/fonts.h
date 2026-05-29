#ifndef fonts_h
#define fonts_h

VOID EFIAPI fonts_init(VOID);
VOID EFIAPI fonts_set_pos(INTN x, INTN y);
VOID EFIAPI fonts_set_inv(BOOLEAN inv);
VOID EFIAPI fonts_set_color(UINT32 color);
VOID EFIAPI fonts_print_str(CHAR8 *str);

#endif /* fonts_h */
