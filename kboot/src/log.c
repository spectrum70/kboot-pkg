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
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>

VOID EFIAPI log_out(IN CONST CHAR16 *pfx, IN CONST CHAR16 *msg, IN VA_LIST list)
{
	CHAR16 buffer[256];

	UnicodeVSPrint(buffer, sizeof(buffer), msg, list);
  	Print(L"%s%s\n", pfx, buffer);
}

VOID EFIAPI err(IN CONST CHAR16 *msg, ...)
{
	VA_LIST list;

	VA_START(list, msg);
	log_out(L"+++err: ", msg, list);
	VA_END(list);
}

VOID EFIAPI log(IN CONST CHAR16 *msg, ...)
{
	VA_LIST list;

	VA_START(list, msg);
	log_out(L"", msg, list);
	VA_END(list);
}

VOID EFIAPI dbg(IN CONST char *f, IN CONST CHAR16 *msg, ...)
{
	VA_LIST list;

	Print(L"%a(): ", f);

	VA_START(list, msg);
	log_out(L"", msg, list);
	VA_END(list);
}
