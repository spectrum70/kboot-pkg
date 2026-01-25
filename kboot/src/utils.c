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

#include "log.h"
#include "utils.h"

#define SIZE_KILO	1024
#define SIZE_MEGA	(SIZE_KILO * 1024)
#define SIZE_GIGA	(SIZE_MEGA * 1024)

static VOID utils_get_size_human(UINT64 size,
				 UINT32 *b, UINT32 *k, UINT32 *m, UINT32 *g)
{
	if (size) { *b = size % 1024; size /= 1024; }
	if (size) { *k = size % 1024; size /= 1024; }
	if (size) { *m = size % 1024; size /= 1024; }
	if (size) { *g = size % 1024; size /= 1024; }
}

VOID utils_print_size(IN UINT64 size, IN enum p_mode mode)
{
	UINT32 b = 0, k = 0, m = 0, g = 0;

	/* Lenovo 34856034304 */

	if (mode == P_MEGA) {
		size /= SIZE_MEGA;
		Print(L"%dM", size);
	} else {
		utils_get_size_human(size, &b, &k, &m, &g);

		if (g)
			Print(L"%dG", g);
		if (m)
			Print(L", %dM", m);
		if (k)
			Print(L", %dK", k);
		if (b)
			Print(L", %dB", b);
	}
}

VOID utils_print_size_buff(OUT CHAR8 *buff, IN UINT64 size, IN enum p_mode mode)
{
	UINT32 b = 0, k = 0, m = 0, g = 0, n = 0;

	if (mode == P_MEGA) {
		size /= SIZE_MEGA;
		AsciiSPrint(buff, 32, "%dM", size);
	} else {
		utils_get_size_human(size, &b, &k, &m, &g);

		if (g)
			n = AsciiSPrint(buff, 32, "%dG", g);
		if (m)
			n += AsciiSPrint(&buff[n], 32, "%dM", m);
		if (k)
			n += AsciiSPrint(&buff[n], 32, "%dK", k);
		if (b)
			AsciiSPrint(&buff[n], 32, "%dB", b);
	}
}

INTN EFIAPI utils_compare_efi_time(IN EFI_TIME *time1, IN EFI_TIME *time2)
{
	if (time1->Year != time2->Year)
		return (time1->Year - time2->Year);
	if (time1->Month != time2->Month)
		return (time1->Month - time2->Month);
	if (time1->Day != time2->Day)
		return (time1->Day - time2->Day);
	if (time1->Hour != time2->Hour)
		return (time1->Hour - time2->Hour);
	if (time1->Minute != time2->Minute)
		return (time1->Minute - time2->Minute);
	return (time1->Second - time2->Second);
}
