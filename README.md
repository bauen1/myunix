# DEPRECATED

See https://gitlab.com/myunix/myunix/ for the next version.

# myunix
A unix-like kernel+userspace, for SCIENCE!

# build
1. build the toolchain

this will build the tcc (tiny c compiler), gcc, bintuils and grub.

```
./build_toolchain.sh
```

2. build the kernel

this will build a bootable iso / cdrom

```
make -C kernel TARGET=myunix-i686 iso
```

# License
See `LICENCE.md`
