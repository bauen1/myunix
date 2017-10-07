#!/bin/bash
set -ex

echo "Cleaning up old toolchain:"
rm -rf toolchain

mkdir toolchain
export PREFIX="$PWD/toolchain"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

cd toolchain
mkdir src
cd src

## binutils

BINUTILS_VERSION=2.29
echo "Downloading binutils-$BINUTILS_VERSION"
wget ftp://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz{,.sig}
tar -xvf binutils-$BINUTILS_VERSION.tar.xz
mkdir build-binutils
cd build-binutils
echo "Building binutils"
../binutils-$BINUTILS_VERSION/configure --target="$TARGET" --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
echo "Installing binutils"
make install
cd ..
echo "Cleaning up source and build directory"
rm -rf build-binutils "binutils-$BINUTILS_VERSION"

## gcc

GCC_VERSION=7.2.0
echo "Downloading gcc-$GCC_VERSION"
wget ftp://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz{,.sig}
tar -xvf gcc-$GCC_VERSION.tar.xz
mkdir gcc-build
cd gcc-build
echo "Building gcc"
../gcc-$GCC_VERSION/configure --target="$TARGET" --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc
echo "Building libgcc"
make all-target-libgcc
echo "Installing gcc"
make install-gcc
echo "Installing libgcc"
make install-target-libgcc
cd ..
echo "Cleaning up source and build directory"
rm -rf gcc-build "gcc-$GCC_VERSION"

## grub
GRUB_VERSION=2.02
echo "Downloading grub-$GRUB_VERSION"
wget ftp://ftp.gnu.org/gnu/grub/grub-$GRUB_VERSION.tar.xz{,.sig}
tar -xvf grub-$GRUB_VERSION.tar.xz
mkdir grub-build
cd grub-build
echo "Building grub"
../grub-$GRUB_VERSION/configure \
  --sbindir="$PREFIX/bin" \
  --prefix="$PREFIX" TARGET_CC=${TARGET}-gcc TARGET_OBJCOPY=${TARGET}-objcopy TARGET_STRIP=${TARGET}-strip TARGET_NM=${TARGET}-nm TARGET_RANLIB=${TARGET}-ranlib --target=${TARGET}
make
echo "Installing grub"
make install
cd ..
echo "Cleaning up source and build directory"
rm -rf grub-build "grub-$GRUB_VERSION"

cd ..

