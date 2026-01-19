#ifndef menu_h
#define menu_h

struct point {
	UINTN x;
	UINTN y;
};

EFI_STATUS menu_exec(IN EFI_HANDLE img_handle);

#endif /* menu_h */
