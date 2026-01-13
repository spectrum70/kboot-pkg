#ifndef log_h
#define log_h

VOID EFIAPI dbg(IN CONST char *f, IN CONST CHAR16 *msg, ...);
VOID EFIAPI err(IN CONST CHAR16 *msg, ...);
VOID EFIAPI log(IN CONST CHAR16 *msg, ...);

#endif /* log_h */
