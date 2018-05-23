CC:=./toolchain/opt/bin/i386-tcc
LD:=$(CC)

init.o: init.c
	$(CC) $(CFLAGS) -g -c $< -o $@

init: init.o
	$(LD) $(LDFLAGS) -Wl,-Ttext=0x1000000 -static -g -o $@ $< $(AFTER_LDFLAGS)

tar_root/init: init.o
	$(LD) $(LDFLAGS) -Wl,-Ttext=0x1000000 -static -Wl,--oformat=binary -o $@ $< $(AFTER_LDFLAGS)

kernel/iso/modules/initrd.tar: tar_root tar_root/init
	cd $< && find . -print0 | cpio --create -0 -v --format=ustar > ../$@

.PHONY: all
all: kernel/iso/modules/initrd.tar
.PHONY: clean
clean:
	rm -f init.o kernel/iso/modules/initrd.tar
