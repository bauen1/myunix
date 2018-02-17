.SUFFIXES:
#
BUILD?=$(shell uname -s | tr '[:upper:]' '[:lower:]')-$(shell uname -m)
HOST?=$(BUILD)
TARGET?=$(HOST)
TARGET_ARCH:=$(lastword $(subst -, ,$(TARGET)))
ARCHDIR=arch/$(TARGET_ARCH)

# default flags
CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=-g
LIBS?=
ASFLAGS?=-g

DESTDIR?=
PREFIX?=/usr/local
BOOTDIR?=/boot
INCLUDEDIR?=$(PREFIX)/include/myunix

# mandatory flags
CFLAGS:=$(CFLAGS) -Wall -Wextra -ffreestanding -std=gnu99 -fstack-protector -Wno-attributes -Wno-format
CFLAGS+=-Werror
CPPFLAGS:=$(CPPFLAGS) -D__myunix_kernel -I./include -I$(ARCHDIR)
LDFLAGS:=$(LDFLAGS) -static
LIBS:=$(LIBS) -nostdlib
ASFLAGS:=$(ASFLAGS) -f elf -w+orphan-labels
OBJS:=$(subst .c,.o,$(wildcard *.c)) $(subst .s,.o,$(wildcard *.s))

# default tools

AS:=nasm
CC:=gcc
#CC:=tcc
LD:=ld
OBJCOPY:=objcopy

# try to use ../toolchain or user-supplied TOOLCHAIN_PREFIX

ifneq ("","$(wildcard ../toolchain/bin)")
  $(info Trying to use toolchain in ../toolchain; pass TOOLCHAIN_PREFIX='' to use the system's c compiler overwrite)
  TOOLCHAIN_PREFIX?=$(PWD)/../toolchain/bin/$(TARGET_ARCH)-elf-
else
  $(info toolchain not build, falling back to system c compiler)
endif

ifdef TOOLCHAIN_PREFIX
  CC:=$(TOOLCHAIN_PREFIX)$(CC)
  LD:=$(TOOLCHAIN_PREFIX)$(LD)
  OBJCOPY:=$(TOOLCHAIN_PREFIX)$(OBJCOPY)
endif

# arch-specific flags
-include $(ARCHDIR)/make.config
CFLAGS:=$(CFLAGS) $(ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(ARCH_LDFLAGS)
LIBS:=$(ARCH_LIBS) $(LIBS)
OBJS:=$(ARCH_OBJS) $(OBJS)

# command line overrides
CFLAGS:=$(CFLAGS) $(EXTRA_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(EXTRA_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(EXTRA_LDFLAGS)
LIBS:=$(LIBS) $(EXTRA_LIBS)
ASFLAGS:=$(ASFLAGS) $(EXTRA_ASFLAGS)
OBJS:=$(OBJS) $(EXTRA_OBJS)

#

DEPS:=$(OBJS:.o=.d)

.DEFAULT: all
.PHONY: all
all: kernel

.PHONY: clean
clean:
	rm -f kernel.elf kernel.sym kernel.bin
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(DEPS) *.d */*.d */*/*.d
	rm -f iso/*.iso iso/kernel.bin

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -MD -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) -MF $(@:.o=.d) $< -o $@

.PHONY: kernel
kernel: kernel.elf kernel.sym kernel.bin

# link using gcc / ld
kernel.elf: $(OBJS) $(ARCHDIR)/link.ld
	$(LD) $(LDFLAGS) $(LIBS) -T $(ARCHDIR)/link.ld -o $@ $(OBJS)
# link using tcc
#kernel.elf: $(OBJS)
#	$(LD) $(LDFLAGS) $(LIBS) -o $@ -nostdinc -Wl,--image-base=0x100000 -Wl,--section-alignment=0x1000 $(OBJS)

kernel.sym: kernel.elf
	$(OBJCOPY) --only-keep-debug $^ $@

kernel.bin: kernel.elf
	$(OBJCOPY) --strip-debug $^ $@

iso/kernel.bin: kernel.bin
	cp $< $@

iso/grub_eltorito.iso: iso/grub/grub.cfg iso/kernel.bin iso/
	grub-mkimage -c iso/grub/grub.cfg -v -p \(cd\)/grub/ -o $@ -O i386-pc-eltorito biosdisk iso9660 part_msdos multiboot boot echo normal

iso/grub_iso.iso: iso/grub_eltorito.iso iso
	rm -f $@
	xorriso -as mkisofs -o $@ -b grub_eltorito.iso -r iso -no-emul-boot -boot-info-table

.PHONY: iso
iso: iso/grub_iso.iso

# virtual machine targets

ifeq ("$(TARGET_ARCH)","i686")
  QEMU?=qemu-system-i386
else
  QEMU?=qemu-system-$(TARGET_ARCH)
endif

QEMUFLAGS?=-serial stdio -no-shutdown -no-reboot $(ARCH_QEMUFLAGS) $(EXTRA_QEMUFLAGS)

.PHONY: run-grub
run-grub: iso/grub_iso.iso
	$(QEMU) $(QEMUFLAGS) -cdrom $<

.PHONY: run-qemu
run-qemu: kernel.bin
	$(QEMU) $(QEMUFLAGS) -kernel kernel.bin

.PHONY: run-debug
run-debug: kernel.bin
	$(QEMU) $(QEMUFLAGS) -s -S -kernel kernel.bin

.PHONY: run
run: run-qemu

.PHONY: gdb
gdb: kernel.elf .gdbinit
	gdb kernel.elf -e .gdbinit

# tests

.PHONY: cppcheck-tcc
cppcheck-tcc:
	cppcheck --enable=all $(CPPFLAGS) $(wildcard *.c) $(widlcard $(ARCHDIR)/*.c) -I ../toolchain/opt/lib/tcc/include $(EXTRA_CPPCHECKFLAGS)

#

-include $(DEPS)
