#!/bin/sh
set -x
echo "copying files to /boot ..."
sudo cp Build/kboot-pkg/DEBUG_GCC5/X64/kboot.efi /boot/EFI/kboot/kboot_x64.efi
sudo cp kboot-pkg/kboot/images/kboot.bmp /boot/EFI/kboot/images
echo "done."
