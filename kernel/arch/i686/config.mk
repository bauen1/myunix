ARCH_CFLAGS:=-mno-red-zone
ARCH_CPPFLAGS:=
ARCH_LDFLAGS:=
ARCH_LIBS:=

# boot.o must be the first object in ARCH_OBJS to link using tcc
ARCH_OBJS:= \
$(ARCHDIR)/boot.o \
$(ARCHDIR)/cpu.o \
$(ARCHDIR)/gdt_flush.o \
$(ARCHDIR)/tss_flush.o \
$(ARCHDIR)/idt_load.o \
$(ARCHDIR)/isrs.o \
$(ARCHDIR)/paging.o \
$(ARCHDIR)/task.o \
$(ARCHDIR)/memcpy.o \
$(ARCHDIR)/mmio.o \
$(ARCHDIR)/atomic.o

ARCH_SRCS:= \
$(ARCHDIR)/cpu.c \
$(ARCHDIR)/mmio.c \
$(ARCHDIR)/memcpy.c \
$(ARCHDIR)/atomic.c

ARCH_LDSCRIPT:= $(ARCHDIR)/link.ld
