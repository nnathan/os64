#!/bin/sh

set -e # errors are fatal

ROOT=`pwd`
TMP=/tmp/os64-tools/
XCC=gcc
XCFLAGS="-Wno-implicit-int -Wno-implicit-function-declaration -fno-builtin"
IMAGEBLKS=25600 # target disk size in 4K blocks (100MB)
CFLAGS=

############################################################

echo ........................................ building cross tools

rm -rf $TMP
mkdir -p $TMP

$XCC $XCFLAGS -o $TMP/as bin/as/*.c

$XCC $XCFLAGS -o $TMP/cc \
	-DBINDIR=\"$TMP\" \
	-DINCDIR=\"$ROOT/include\" \
	-DLIBDIR=\"$ROOT/lib\" \
	bin/cc.c

$XCC $XCFLAGS -o $TMP/cc1 bin/cc1/*.c
$XCC $XCFLAGS -o $TMP/cpp bin/cpp/*.c
$XCC $XCFLAGS -o $TMP/ld bin/ld.c
$XCC $XCFLAGS -o $TMP/mkboot bin/mkboot.c
$XCC $XCFLAGS -o $TMP/mkfs bin/mkfs.c
$XCC $XCFLAGS -o $TMP/obj bin/obj.c

AS=$TMP/as
CC=$TMP/cc
LD=$TMP/ld
MKBOOT=$TMP/mkboot
MKFS=$TMP/mkfs
OBJ=$TMP/obj

############################################################

echo ........................................ building libc

$AS -o lib/libc/bzero.o -l lib/libc/bzero.lst lib/libc/bzero.s

############################################################

echo ........................................ building kernel

$AS -o kernel/locore.o -l kernel/locore.lst kernel/locore.s
$AS -o kernel/lib.o -l kernel/lib.lst kernel/lib.s

$CC $CFLAGS -D_KERNEL -c kernel/main.c
$CC $CFLAGS -D_KERNEL -c kernel/cons.c
$CC $CFLAGS -D_KERNEL -c kernel/page.c
$CC $CFLAGS -D_KERNEL -c kernel/sched.c
$CC $CFLAGS -D_KERNEL -c kernel/seg.c
$CC $CFLAGS -D_KERNEL -c kernel/acpi.c
$CC $CFLAGS -D_KERNEL -c kernel/clock.c
$CC $CFLAGS -D_KERNEL -c kernel/slab.c

$LD -o kernel/kernel -e start -b 0x1000 \
	kernel/locore.o kernel/lib.o kernel/main.o kernel/cons.o \
	kernel/page.o kernel/sched.o kernel/seg.o kernel/acpi.o \
	kernel/clock.o kernel/slab.o \
	lib/libc/bzero.o

$OBJ -s kernel/kernel >kernel/kernel.map

############################################################

echo ........................................ building boot block

$AS -o boot.o boot.s
$LD -o boot -r -b 0 boot.o
dd if=boot of=boot.bin bs=4k count=1
rm boot.o boot

echo ........................................ building disk image

dd if=/dev/zero of=image bs=4K count=$IMAGEBLKS # 100 MB disk
$MKFS image $IMAGEBLKS proto
$MKBOOT -b boot.bin image
