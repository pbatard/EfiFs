/* boot.c - Simple gnu-efi application for testing EfiFs */
/*
 *  Copyright © 2026 Pete Batard <pete@akeo.ie>
 *
 *  Note: This file has been relicensed from GPLv3+ to GPLv2+ by formal
 *  agreement of all of its contributors.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <efi.h>
#include <efilib.h>

#if defined(_M_X64) || defined(__x86_64__)
static CHAR16* ArchName = u"x64";
#elif defined(_M_IX86) || defined(__i386__)
static CHAR16* ArchName = u"ia32";
#elif defined (_M_ARM64) || defined(__aarch64__)
static CHAR16* ArchName = u"aa64";
#elif defined (_M_ARM) || defined(__arm__)
static CHAR16* ArchName = u"arm";
#elif defined (_M_RISCV64) || (defined(__riscv) && (__riscv_xlen == 64))
static CHAR16* ArchName = u"riscv64";
#elif defined (_M_LOONGARCH64) || defined(__loongarch64)
static CHAR16* ArchName = u"loongarch64";
#else
#error Unsupported architecture
#endif

EFI_STATUS
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	INTN Argc, i;
	UINTN Event;
	CHAR16 **Argv, *FsName = u"EfiFs";

	InitializeLib(ImageHandle, SystemTable);
	Argc = GetShellArgcArgv(ImageHandle, &Argv);
	for (i = 1; i < Argc; i++) {
		if (StrCmp(Argv[i], u"-test") == 0) {
			Print(u"Test Passed\n");
			return EFI_SUCCESS;
		}
		FsName = Argv[i];
	}

	Print(u"\n%H*** Hello from %s (%s)! ***%N\n\n", FsName, ArchName);

	Print(u"%EPress any key to exit.%N\n");
	SystemTable->ConIn->Reset(SystemTable->ConIn, FALSE);
	SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &Event);
	SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);

	return EFI_SUCCESS;
}
