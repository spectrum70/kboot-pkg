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

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PciLib.h>
#include <Library/UefiLib.h>

#include "log.h"

// SMN (System Management Network) Access Registers
#define AMD_SMN_ADDR_REG    PCI_LIB_ADDRESS(0, 24, 3, 0xB8)
#define AMD_SMN_DATA_REG    PCI_LIB_ADDRESS(0, 24, 3, 0xBC)

// Thermal Sensor SMN Address for modern Ryzen (Zen 2+)
// Offset 0x59800 is a standard starting point for Tctl/Tdie
#define AMD_THM_TCTL_ADDR 0x00059B08

/* TODO: not working, reads 0 */
EFI_STATUS EFIAPI cpu_print_amd_ryzen_cpu_temp(VOID)
{
	UINT32 tctl_raw;
	INT32 temp_in_celsius;

	PciWrite32(AMD_SMN_ADDR_REG, AMD_THM_TCTL_ADDR);
	tctl_raw = PciRead32(AMD_SMN_DATA_REG);

	temp_in_celsius = (INT32)((tctl_raw >> 21) & 0x7FF) / 8;

	Print(L"CPU Temperature: %d C\n", temp_in_celsius);

	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI cpu_print_cpu_id(VOID)
{
	UINT32 eax;
	CHAR8 str_brand[49];
	UINT32 *ptr;

	ZeroMem(str_brand, sizeof(str_brand));
	ptr = (UINT32 *)str_brand;

	// Check if extended CPUID leaves are supported
	AsmCpuid(0x80000000, &eax, NULL, NULL, NULL);
	if (eax < 0x80000004) {
		err(L"Extended CPUID brand string not supported.\n");
		return EFI_UNSUPPORTED;
	}

	// Retrieve the Brand String (stored in 3 consecutive leaves)
	// Leaf 0x80000002
	AsmCpuid(0x80000002, &ptr[0], &ptr[1], &ptr[2], &ptr[3]);
	// Leaf 0x80000003
	AsmCpuid(0x80000003, &ptr[4], &ptr[5], &ptr[6], &ptr[7]);
	// Leaf 0x80000004
	AsmCpuid(0x80000004, &ptr[8], &ptr[9], &ptr[10], &ptr[11]);

	// Print the result (using %a for CHAR8/ASCII string)
	Print(L"CPU: %a\n", str_brand);

	return EFI_SUCCESS;
}
