#
BUILD?=$(shell uname -s | tr '[:upper:]' '[:lower:]')-$(shell uname -m)
HOST?=$(BUILD)
TARGET?=$(HOST)
TARGET_ARCH:=$(lastword $(subst -, ,$(TARGET)))
ifeq ($(TARGET_ARCH),x86_64)
  TARGET_ARCH:=i686
endif
ARCHDIR=arch/$(TARGET_ARCH)

# default flags
CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=
ASFLAGS?=

DESTDIR?=
PREFIX?=/usr/local
BOOTDIR?=/boot
INCLUDEDIR?=$(PREFIX)/include/myunix

# mandatory flags
CFLAGS:=$(CFLAGS) -Wall -Wextra -I$(PWD)/include -ffreestanding -std=gnu99 -fstack-protector-all -I./include -I$(ARCHDIR)
CPPFLAGS:=$(CPPFLAGS) -D__myunix_kernel
LDFLAGS:=$(LDFLAGS) -ffreestanding
LIBS:=$(LIBS) -nostdlib
ASFLAGS:=$(ASFLAGS) -f elf -w+orphan-labels
OBJS:=$(subst .c,.o,$(wildcard *.c)) $(subst .s,.o,$(wildcard *.s))

# arch-specific flags
include $(ARCHDIR)/make.config
CFLAGS:=$(CFLAGS) $(ARCH_CFLAGS)
CPPFLAGS:=$(CFLAGS) $(ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(ARCH_LDFLAGS)
LIBS:=$(LIBS) $(ARCH_LIBS)
OBJS:=$(OBJS) $(ARCH_OBJS)

# default tools

AS:=nasm
CC:=gcc
LD:=ld

# try to use ../toolchain or user-supplied TOOLCHAIN_PREFIX

ifneq ("","$(wildcard ../toolchain/bin)")
  $(info Trying to use toolchain in ../toolchain; pass TOOLCHAIN_PREFIX='' to use the system's c compiler overwrite)
  TOOLCHAIN_PREFIX?=$(PWD)/../toolchain/bin/$(TARGET_ARCH)-elf-
else
  $(info toolchain not build, falling back to system c compiler)
endif

ifdef TOOLCHAIN_PREFIX
  CC:=$(TOOLCHAIN_PREFIX)gcc
  LD:=$(TOOLCHAIN_PREFIX)ld
endif

#

DEPS:=$(OBJS:.o=.d)

.DEFAULT: all
.PHONY: all
all: kernel.bin

.PHONY: clean
clean:
	rm -f kernel.bin
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(DEPS) *.d */*.d */*/*.d

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) -MF $(@:.o=.d) $< -o $@

kernel.bin: $(OBJS) $(ARCHDIR)/link.ld
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LIBS) -T $(ARCHDIR)/link.ld -o $@ $(OBJS)

# virtual machine targets

ifeq ("$(TARGET_ARCH)","i686")
  QEMU?=qemu-system-i386
else
  QEMU?=qemu-system-$(TARGET_ARCH)
endif

QEMUFLAGS?=-no-shutdown -no-reboot

.PHONY: run
run: kernel.bin
	$(QEMU) $(QEMUFLAGS) -kernel kernel.bin



# install targets

.PHONY: install
install: install-headers install-kernel

.PHONY: install-headers
install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R include/. $(ARCHDIR)/*.h $(DESTDIR)$(INCLUDEDIR)/.

.PHONY: install-kernel
install-kernel: kernel.bin
	mkdir -p $(DESTDIR)$(BOOTDIR)
	install -D -m0755 kernel.bin $(DESTDIR)$(BOOTDIR)/kernel.bin

-include $(DEPS)
