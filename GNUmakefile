#
# This makefile system follows the structuring conventions
# recommended by Peter Miller in his excellent paper:
#
#	Recursive Make Considered Harmful
#	http://aegis.sourceforge.net/auug97.pdf
#
OBJDIR := obj

# Run 'make V=1' to turn on verbose commands, or 'make V=0' to turn them off.
ifeq ($(V),1)
override V =
endif
ifeq ($(V),0)
override V = @
endif

-include conf/lab.mk

-include conf/env.mk

LABSETUP ?= ./

TOP = .

CFLAGS        ?=
EXTRA_CFLAGS  ?=
NATIVE_CFLAGS ?=

ifdef JOSLLVM

CC            := clang
AS            := llvm-as
AR            := llvm-ar
LD            := ld.lld
OBJCOPY       := llvm/gnu-objcopy
OBJDUMP       := llvm-objdump
NM            := llvm-nm

CFLAGS        += -target i386-gnu-linux -march=pentium2 -pipe -DJOS_LLVM=1
EXTRA_CFLAGS  += -Wno-self-assign -Wno-format-nonliteral -Wno-address-of-packed-member -g -gpubnames

GCC_LIB       := $(shell $(CC) $(CFLAGS) -print-resource-dir)/lib/j*s/libclang_rt.builtins-i386.a

else

# Cross-compiler jos toolchain
#
# This Makefile will automatically use the cross-compiler toolchain
# installed as 'i386-jos-elf-*', if one exists.  If the host tools ('gcc',
# 'objdump', and so forth) compile for a 32-bit x86 ELF target, that will
# be detected as well.  If you have the right compiler toolchain installed
# using a different name, set GCCPREFIX explicitly in conf/env.mk

# try to infer the correct GCCPREFIX
ifndef GCCPREFIX
GCCPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i386-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
	then echo ''; \
	elif i686-jetos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i686-jetos-elf-'; \
	elif i386-jetos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i386-jetos-elf-'; \
	else echo "***" 1>&2; \
	echo "*** Warning: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your GCCPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this warning, run 'gmake GCCPREFIX= ...'." 1>&2; \
	echo "*** Perhaps you wanted to use llvm toolchain, in this case ensure" 1>&2; \
	echo "*** JOSLLVM environment variable is defined." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

CC             := $(GCCPREFIX)gcc
AS             := $(GCCPREFIX)as
AR             := $(GCCPREFIX)ar
LD             := $(GCCPREFIX)ld
OBJCOPY        := $(GCCPREFIX)objcopy
OBJDUMP        := $(GCCPREFIX)objdump
NM             := $(GCCPREFIX)nm

CFLAGS         += -fno-pic -pipe
# -fno-tree-ch prevented gcc from sometimes reordering read_ebp() before
# mon_backtrace()'s function prologue on gcc version: (Debian 4.7.2-5) 4.7.2
EXTRA_CFLAGS   += -Wno-unused-but-set-variable -g -gpubnames -fno-tree-ch

GCC_LIB        := $(shell $(CC) $(CFLAGS) -m32 -print-libgcc-file-name)

endif

# Native commands
NCC           := gcc
NATIVE_CFLAGS += -pipe $(DEFS) $(LABDEFS) -I$(TOP) -MD -Wall
TAR           := gtar
PERL          := perl

# try to infer the correct QEMU
ifndef QEMU
QEMU := $(shell if which qemu > /dev/null 2>&1; \
	then echo qemu; exit; \
	elif which qemu-system-i386 > /dev/null 2>&1; \
	then echo $$(which qemu-system-i386); exit; \
	else \
	qemu=/Applications/Q.app/Contents/MacOS/i386-softmmu.app/Contents/MacOS/i386-softmmu; \
	if test -x $$qemu; then echo $$qemu; exit; fi; fi; \
	echo "***" 1>&2; \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2; \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2; \
	echo "*** or have you tried setting the QEMU variable in conf/env.mk?" 1>&2; \
	echo "***" 1>&2; exit 1)
endif

# try to generate a unique GDB port
GDBPORT	:= $(shell expr `id -u` % 5000 + 25000)

# Compiler flags
# -fno-builtin is required to avoid refs to undefined functions in the kernel.
# Only optimize to -O1 to discourage inlining, which complicates backtraces.
CFLAGS += $(DEFS) $(LABDEFS) -O1 -I$(TOP) -MD
CFLAGS += -m32 -fno-builtin -fno-omit-frame-pointer -fno-stack-protector
CFLAGS += -Wall -Wformat=2 -Wno-unused-function -Werror
CFLAGS += $(EXTRA_CFLAGS)

# Common linker flags
LDFLAGS := -m elf_i386

# Linker flags for JOS user programs
ULDFLAGS := -T user/user.ld

# Lists that the */Makefrag makefile fragments will add to
OBJDIRS :=

# Make sure that 'all' is the first target
all: .git/hooks/post-checkout .git/hooks/pre-commit

# Eliminate default suffix rules
.SUFFIXES:

# Delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# make it so that no intermediate .o files are ever deleted
.PRECIOUS: %.o $(OBJDIR)/boot/%.o $(OBJDIR)/kern/%.o \
	   $(OBJDIR)/lib/%.o $(OBJDIR)/fs/%.o $(OBJDIR)/net/%.o \
	   $(OBJDIR)/user/%.o

KERN_CFLAGS := $(CFLAGS) -DJOS_KERNEL
USER_CFLAGS := $(CFLAGS)
ifeq ($(CONFIG_KSPACE),y)
KERN_CFLAGS += -DCONFIG_KSPACE
USER_CFLAGS += -DCONFIG_KSPACE -DJOS_PROG
else
USER_CFLAGS += -DJOS_USER
endif

# Update .vars.X if variable X has changed since the last make run.
#
# Rules that use variable X should depend on $(OBJDIR)/.vars.X.  If
# the variable's value has changed, this will update the vars file and
# force a rebuild of the rule that depends on it.
$(OBJDIR)/.vars.%: FORCE
	@test -f $@ || touch $@
	$(V)echo "$($*)" | cmp -s $@ || echo "$($*)" > $@
.PRECIOUS: $(OBJDIR)/.vars.%
.PHONY: FORCE


# Include Makefrags for subdirectories
include boot/Makefrag
include kern/Makefrag


QEMUOPTS = -drive format=raw,index=0,media=disk,file=$(OBJDIR)/kern/kernel.img -serial mon:stdio -gdb tcp::$(GDBPORT)
QEMUOPTS += $(shell if $(QEMU) -nographic -help | grep -q '^-D '; then echo '-D qemu.log'; fi)
IMAGES = $(OBJDIR)/kern/kernel.img
QEMUOPTS += $(QEMUEXTRA)

define POST_CHECKOUT
#!/bin/sh -x
make clean
endef
export POST_CHECKOUT

define PRE_COMMIT
#!/bin/sh

if git diff --cached --name-only --diff-filter=DMR | grep -q grade
then
   echo "FAIL: Don't change grade files."
   exit 1
else
   exit 0
fi
endef
export PRE_COMMIT

.git/hooks/post-checkout:
	@echo "$$POST_CHECKOUT" > $@
	@chmod +x $@

.git/hooks/pre-commit:
	@echo "$$PRE_COMMIT" > $@
	@chmod +x $@

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

pre-qemu: .gdbinit

qemu: $(IMAGES) pre-qemu
	$(QEMU) $(QEMUOPTS)

qemu-nox: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Use Ctrl-a x to exit qemu"
	@echo "***"
	$(QEMU) -nographic $(QEMUOPTS)

qemu-gdb: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Now run 'gdb'." 1>&2
	@echo "***"
	$(QEMU) $(QEMUOPTS) -S

qemu-nox-gdb: $(IMAGES) pre-qemu
	@echo "***"
	@echo "*** Now run 'gdb'." 1>&2
	@echo "***"
	$(QEMU) -nographic $(QEMUOPTS) -S

print-qemu:
	@echo $(QEMU)

print-gdbport:
	@echo $(GDBPORT)

# For deleting the build
clean:
	rm -rf $(OBJDIR) .gdbinit jos.in qemu.log

realclean: clean
	rm -rf lab$(LAB).tar.gz \
		jos.out $(wildcard jos.out.*) \
		qemu.pcap $(wildcard qemu.pcap.*)

distclean: realclean
	rm -f .git/hooks/pre-commit .git/hooks/post-checkout

ifneq ($(V),@)
GRADEFLAGS += -v
endif

grade:
	@echo $(MAKE) clean
	@$(MAKE) clean || \
	  (echo "'make clean' failed.  HINT: Do you have another running instance of JOS?" && exit 1)
	./grade-lab$(LAB) $(GRADEFLAGS)


# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
$(OBJDIR)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(OBJDIR)/$(dir)/*.d))
	@mkdir -p $(@D)
	@$(PERL) mergedep.pl $@ $^

-include $(OBJDIR)/.deps

always:
	@:

.PHONY: all always clean realclean distclean grade
