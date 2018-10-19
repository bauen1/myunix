.DEFAULT: all
.PHONY: all
all: kernel/iso/modules/initrd.tar userspace/init.elf

CC:=./toolchain/opt/bin/i386-tcc
LD:=$(CC)

CFLAGS=-g

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.elf: %.o
	$(LD) $(LDFLAGS) -Wl,-Ttext=0x1000000 -static -g -o $@ $< $(AFTER_LDFLAGS)

tar_root/init: userspace/init.o userspace/init.elf
	$(LD) $(LDFLAGS) -Wl,-Ttext=0x1000000 -static -Wl,--oformat=binary -o $@ $< $(AFTER_LDFLAGS)

tar_root/bin:
	mkdir -p $@

tar_root/bin/%: userspace/%.o | tar_root/bin
	$(LD) $(LDFLAGS) -Wl,-Ttext=0x1000000 -static -Wl,--oformat=binary -o $@ $< $(AFTER_LDFLAGS)

kernel/iso/modules/initrd.tar: tar_root tar_root/init tar_root/bin/test
	cd $< && find . -print0 | cpio --create -0 -v --format=ustar > ../$@

.PHONY: clean
clean:
	rm -f userspace/*.o userspace/*.elf
	rm -f tar_root/bin/*
	rm -f tar_root/init
	rm -f kernel/iso/modules/initrd.tar

.PHONY: update-musl-patch
update-musl-patch:
	git -C toolchain/src/musl diff -p --staged > toolchain/patches/musl.patch

