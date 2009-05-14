#
# Copyright (c) 2008 James Molloy, James Pritchett, Jrg Pfhler, Matthew Iselin
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# Run this in the images/install folder

SRCDIR=../../

echo "Building installer disk image..."

if which mkisofs > /dev/null 2>&1; then
    rm -f pedigree-i386.iso
    cp $SRCDIR/build/src/system/kernel/kernel ./disk/boot
    cp $SRCDIR/build/initrd.tar ./disk/boot
	cp $SRCDIR/build/*.so ./disk/libraries
	cp $SRCDIR/build/src/user/applications/login/login ./disk/applications
    mkisofs -joliet -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o pedigree-i386.iso -V "Pedigree" disk > /dev/null 2>&1
else
    echo Not creating bootable ISO because \`mkisofs\' could not be found.
fi
