#ifndef fb_h
#define fb_h

VOID EFIAPI fb_init(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop);
VOID EFIAPI fb_draw_pixel(UINTN x, UINTN y, UINT32 color);
VOID EFIAPI fb_draw_circle(UINTN x, UINTN y, UINTN radius, UINT32 color);
VOID EFIAPI fb_draw_line(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
			 INTN x0, INTN y0, INTN x1, INTN y1,
			 EFI_GRAPHICS_OUTPUT_BLT_PIXEL color);
#endif /* fb_h */
