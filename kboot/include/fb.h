#ifndef fb_h
#define fb_h

VOID EFIAPI fb_init(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *gop);
VOID EFIAPI fb_draw_circle(UINTN x, UINTN y, UINTN radius, UINT32 color);

#endif /* fb_h */
