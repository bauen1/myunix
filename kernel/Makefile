# disable all default rules
.SUFFIXES:

#
BUILD?=$(shell uname -s | tr '[:upper:]' '[:lower:]')-$(shell uname -m)
HOST?=$(BUILD)
TARGET?=$(HOST)
TARGET_ARCH:=$(lastword $(subst -, ,$(TARGET)))
ARCHDIR=arch/$(TARGET_ARCH)

# default flags
CFLAGS?=-O2 -g -Wall -Wextra -Werror -Wno-attributes -Wno-format -Wcast-align
CPPFLAGS?=
LDFLAGS?=-g
LIBS?=
ASFLAGS?=-g

# mandatory flags
# -fno-omit-frame-pointer is needed to generate a stack trace
CFLAGS:=$(CFLAGS) -ffreestanding -std=gnu99 -fstack-protector -fno-omit-frame-pointer
CPPFLAGS:=$(CPPFLAGS) -D__myunix_kernel -I./include -I./$(ARCHDIR)
LDFLAGS:=$(LDFLAGS) -static -nostdinc
LIBS:=$(LIBS) -nostdlib
ASFLAGS:=$(ASFLAGS) -f elf -w+orphan-labels
SRC_DIRS = . misc usb net
SRCS_GLOBS = $(addsuffix /*.c,$(SRC_DIRS)) $(addsuffix /*.s,$(SRC_DIRS))
SRCS = $(sort $(wildcard $(SRCS_GLOBS)))
OBJS:= $(subst .c,.o,$(subst .s,.o,$(SRCS)))

# default tools
AS:=nasm
CC:=$(TARGET_ARCH)-elf-gcc
#CC:=i386-tcc
OBJCOPY:=$(TARGET_ARCH)-elf-objcopy

# if the user didn't supply TOOLCHAIN_PREFIX try to use ../toolchain and fallback to no toolchain
ifndef TOOLCHAIN_PREFIX
  ifneq ("","$(wildcard ../toolchain/bin)")
    $(info Trying to use toolchain in ../toolchain; pass TOOLCHAIN_PREFIX='' to use the system's c compiler overwrite)
    TOOLCHAIN_PREFIX?=$(PWD)/../toolchain/bin
  else
    $(info toolchain not build, falling back to system c compiler)
  endif
endif

ifdef TOOLCHAIN_PREFIX
  CC:=$(TOOLCHAIN_PREFIX)/$(CC)
  OBJCOPY:=$(TOOLCHAIN_PREFIX)/$(OBJCOPY)
endif

# arch-specific flags
-include $(ARCHDIR)/config.mk

# apply arch specific flags and command line flags
CFLAGS:=$(CFLAGS) $(ARCH_CFLAGS) $(EXTRA_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(ARCH_CPPFLAGS) $(EXTRA_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(ARCH_LDFLAGS) $(EXTRA_LDFLAGS)
LIBS:=$(ARCH_LIBS) $(LIBS) $(EXTRA_LIBS)
OBJS:=$(ARCH_OBJS) $(OBJS) $(EXTRA_OBJS)
DEPS:=$(OBJS:.o=.d)

#
.DEFAULT: all
.PHONY: all
all: iso

.PHONY: iso
iso: iso/grub_iso.iso

.PHONY: clean
clean:
	rm -f kernel.elf kernel.sym kernel.bin
	@rm -f $(OBJS) *.o */*.o */*/*.o
	@echo rm -f objs
	@rm -f $(DEPS) *.d */*.d */*/*.d
	@echo rm -f deps
	rm -f iso/*.iso iso/kernel.bin

%.o: %.c
	@$(CC) $(CFLAGS) $(CPPFLAGS) -MD -c $< -o $@
	@echo CC $< -o $@

%.o: %.s
	@$(AS) $(ASFLAGS) -MF $(@:.o=.d) $< -o $@
	@echo NASM $< -o $@

.PHONY: kernel
kernel: kernel.elf kernel.sym kernel.bin

# TODO: this should probably be in arch/*/config.mak (partially)
# link using gcc / tcc
kernel.elf: $(OBJS) $(ARCHDIR)/link.ld
	@$(CC) $(LDFLAGS) $(LIBS) -T $(ARCHDIR)/link.ld -o $@ $(OBJS)
	@echo LD $< -o $@
# link using tcc
#kernel.elf: $(OBJS)
#	@$(CC) $(LDFLAGS) $(LIBS) -Wl,-Ttext=0x100000 -Wl,--section-alignment=0x1000 -o $@ $(OBJS)
#	@echo LD $< -o $@

# optional
kernel.sym: kernel.elf
	@$(OBJCOPY) --only-keep-debug $^ $@
	@echo OBJCOPY --only-keep-debug $< -o $@

# objcopy
kernel.bin: kernel.elf
	@$(OBJCOPY) --strip-debug $^ $@
	@echo OBJCOPY --strip-debug $< -o $@
# link again using tcc
#kernel.bin: kernel.elf
#	@$(CC) $(LDFLAGS) $(LIBS) -Wl,-Ttext=0x100000 -Wl,--section-alignment=0x1000 -Wl,--oformat=binary -o $@ $(OBJS)
#	@echo LD --oformat=binary $< -o $@

iso/kernel.bin: kernel.bin
	cp $< $@

iso/grub_eltorito.iso: iso/grub/grub.cfg iso/kernel.bin iso/
	grub-mkimage -c iso/grub/grub.cfg -v -p \(cd\)/grub/ -o $@ -O i386-pc-eltorito biosdisk iso9660 part_msdos multiboot boot echo normal all_video 915resolution vga video videoinfo gfxterm

iso/grub_iso.iso: iso/grub_eltorito.iso iso
	rm -f $@
	xorriso -as mkisofs -o $@ -b grub_eltorito.iso -r iso -no-emul-boot -boot-info-table

# virtual machine targets
ifeq ("$(TARGET_ARCH)","i686")
  QEMU?=qemu-system-i386
else
  QEMU?=qemu-system-$(TARGET_ARCH)
endif

QEMUFLAGS?=-serial stdio -no-shutdown -no-reboot $(ARCH_QEMUFLAGS) $(EXTRA_QEMUFLAGS)

.PHONY: run
run: run-grub

.PHONY: run-grub
run-grub: iso/grub_iso.iso
	$(QEMU) $(QEMUFLAGS) -cdrom $<

.PHOYN: debug-grub
debug-grub: iso/grub_iso.iso
	$(QEMU) $(QEMUFLAGS) -cdrom $< -s -S

# tests

.PHONY: cppcheck-tcc
cppcheck-tcc:
	cppcheck --quiet --enable=all $(CPPFLAGS) $(SRCS_GLOBS) -I ../toolchain/opt/lib/tcc/include $(EXTRA_CPPCHECKFLAGS)

.PHONY: codespell
codespell:
	codespell -w -c --interactive=3 --skip=*.o --skip=*.d .

.PHONY: complexity
complexity:
	complexity -h -c $(SRCS)

#

.PHONY: todo
todo:
	grep --color=always "TODO" -r .
	grep --color=always "XXX" -r .
	grep --color=always "FIXME" -r .

#

-include $(DEPS)
