.DEFAULT: all
.PHONY: all
all: kernel/iso/modules/initrd.tar init.elf

CC:=./toolchain/opt/bin/i386-tcc
LD:=$(CC)

CFLAGS=-g

init.o: init.c
	$(CC) $(CFLAGS) -c $< -o $@

init.elf: init.o
	$(LD) $(LDFLAGS) -Wl,-Ttext=0x1000000 -static -g -o $@ $< $(AFTER_LDFLAGS)

tar_root/init: init.o
	$(LD) $(LDFLAGS) -Wl,-Ttext=0x1000000 -static -Wl,--oformat=binary -o $@ $< $(AFTER_LDFLAGS)

kernel/iso/modules/initrd.tar: tar_root tar_root/init
	cd $< && find . -print0 | cpio --create -0 -v --format=ustar > ../$@

.PHONY: clean
clean:
	rm -f init.o init.elf
	rm -f kernel/iso/modules/initrd.tar

.PHONY: update-musl-patch
update-musl-patch:
	git -C toolchain/src/musl diff -p --staged > toolchain/patches/musl.patch

