#!/bin/sh
set -e
set -x

BINUTILS_VERSION=2.30
GCC_VERSION=7.3.0
GRUB_VERSION=2.02
TINYCC_TAG=release_0_9_27
PERL_VERSION=5.20.3
MUSL_VERSION=v1.1.19

mkdir -p toolchain
export PREFIX="$PWD/toolchain"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
export JOBS=4

cd toolchain
mkdir -p src
cd src

if [ ! -f .downloaded_binutils ]; then
	echo "Downloading binutils-$BINUTILS_VERSION"
	wget ftp://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz
	wget ftp://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz.sig
	tar -xvf binutils-$BINUTILS_VERSION.tar.xz
	touch .downloaded_binutils
fi
if [ ! -f .downloaded_gcc ]; then
	echo "Downloading gcc-$GCC_VERSION"
	wget ftp://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz
	wget ftp://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz.sig
	tar -xvf gcc-$GCC_VERSION.tar.xz
	touch .downloaded_gcc
fi
if [ ! -f .downloaded_grub ]; then
	echo "Downloading grub-$GRUB_VERSION"
	wget ftp://ftp.gnu.org/gnu/grub/grub-$GRUB_VERSION.tar.xz
	wget ftp://ftp.gnu.org/gnu/grub/grub-$GRUB_VERSION.tar.xz.sig
	tar -xvf grub-$GRUB_VERSION.tar.xz
	touch .downloaded_grub
fi
if [ ! -f .downloaded_tinycc ]; then
	echo "Cloning tinycc"
	test -d tinycc || git clone "http://repo.or.cz/tinycc.git"
	git -C tinycc checkout $TINYCC_TAG
	git -C tinycc apply ../../patches/tinycc_output_bss.patch
	touch .downloaded_tinycc
fi
if [ ! -f .downloaded_perl ]; then
	echo "Downloading perl-$PERL_VERSION"
	wget "http://www.cpan.org/src/5.0/perl-$PERL_VERSION.tar.gz"
	tar -xvf perl-$PERL_VERSION.tar.gz
	touch .downloaded_perl
fi
if [ ! -f .downloaded_musl ]; then
	echo "Cloning musl"
	test -d musl || git clone "git://git.musl-libc.org/musl"
	git -C musl checkout -- .
	git -C musl checkout $MUSL_VERSION
	git -C musl apply ../../patches/musl.patch
	touch .downloaded_musl
fi

# Compile

if [ ! -f .built_binutils ]; then
	echo "Building Binutils"
	rm -rf binutils-build
	mkdir -p binutils-build
	(cd binutils-build
		../binutils-$BINUTILS_VERSION/configure \
			--target="$TARGET" \
			--prefix="$PREFIX" \
			--with-sysroot \
			--disable-nls \
			--disable-werror

		make -j$JOBS
		echo "Installing binutils"
		make -j$JOBS install
	)
	echo "Cleaning up"
	rm -rf binutils-build
	#"binutils-$BINUTILS_VERSION"
	touch .built_binutils
fi

if [ ! -f .built_tinycc ]; then
	echo "Building Tiny C Compiler"
	rm -rf tinycc-build
	mkdir -p tinycc-build
	(cd tinycc-build
		../tinycc/configure \
			--prefix="$PREFIX/opt" \
			--strip-binaries \
			--sysroot='$PREFIX/sysroot' \
			--config-musl

		#../configure \
		#	--prefix="$PREFIX" \
		#	--enable-cross \
		#	--sysincludepaths='' \
		#	--libpaths='' \
		#	--crtprefix=''
		make -j$JOBS cross-i386
#		make -j$JOBS cross-x86_64
#		make -j$JOBS cross-arm
#		make -j$JOBS cross-arm64
		make -j$JOBS install
	)

	echo "Cleaning up build directory"
	rm -rf tinycc-build
	touch .built_tinycc
fi

if [ ! -f .built_gcc ]; then
	echo "Building gcc"
	rm -rf gcc-build
	mkdir -p gcc-build
	(cd gcc-build
		../gcc-$GCC_VERSION/configure \
			--target="$TARGET" \
			--prefix="$PREFIX" \
			--disable-nls \
			--enable-languages=c \
			--without-headers
		make -j$JOBS all-gcc all-target-libgcc
		make -j$JOBS install-gcc install-target-libgcc
	)

	echo "Cleaning up build directory"
	rm -rf gcc-build
	touch .built_gcc
fi

if [ ! -f .built_grub ]; then
	echo "Building grub"
	rm -rf grub-build
	mkdir -p grub-build
	(cd grub-build
		../grub-$GRUB_VERSION/configure \
			--sbindir="$PREFIX/bin" \
			--prefix="$PREFIX" \
			TARGET_CC=${TARGET}-gcc \
			TARGET_OBJCOPY=${TARGET}-objcopy \
			TARGET_STRIP=${TARGET}-strip \
			TARGET_NM=${TARGET}-nm \
			TARGET_RANLIB=${TARGET}-ranlib \
			--target=${TARGET}
		make -j$JOBS
		make -j$JOBS install
	)
	echo "Cleaning up source and build directory"
	rm -rf grub-build
	touch .built_grub
fi

if [ ! -f .built_perl ]; then
	echo "Building grub"
	(cd perl-$PERL_VERSION
		./Configure \
			-des -Dprefix="$PREFIX"
		make -j$JOBS
		make -j$JOBS install
	)
	touch .built_perl
fi

if [ ! -f .built_musl ]; then
	echo "Building musl"
	rm -rf musl-build
	mkdir -p musl-build
	rm -rf ../musl-install || true
	(cd musl-build
		make -C ../musl distclean
		CROSS_COMPILE="$PWD/../../../toolchain/bin/i686-elf-" \
		CC="$PWD/../../../toolchain/opt/bin/i386-tcc" \
		CFLAGS="-I$PWD/../../../toolchain/opt/lib/tcc/include/ -g" \
		../musl/configure \
			--prefix=/usr \
			--target=i386-myunix \
			--host=i386-myunix \
			--disable-shared
		make -j$JOBS all
		make -j$JOBS install DESTDIR="$PWD/../../musl-install"
	)

	echo "Cleaning up build directory"
	rm -rf musl-build
	touch .built_musl
	echo "Linking toolchain/sysroot to installed musl libc"
	ln -svf musl-install ../sysroot
fi
