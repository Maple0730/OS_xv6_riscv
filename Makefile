K=kernel
U=user
M=mkfs
B=build
BK=$(B)/kernel
BU=$(B)/user
BM=$(B)/mkfs
FSIMG=$(B)/fs.img
KERNEL=$(BK)/kernel

OBJS = \
  $(BK)/entry.o \
  $(BK)/start.o \
  $(BK)/console.o \
  $(BK)/printf.o \
  $(BK)/uart.o \
  $(BK)/kalloc.o \
  $(BK)/kmalloc.o \
  $(BK)/spinlock.o \
  $(BK)/string.o \
  $(BK)/main.o \
  $(BK)/vm.o \
  $(BK)/proc.o \
  $(BK)/swtch.o \
  $(BK)/trampoline.o \
  $(BK)/trap.o \
  $(BK)/syscall.o \
  $(BK)/sysproc.o \
  $(BK)/sem.o \
  $(BK)/shm.o \
  $(BK)/bio.o \
  $(BK)/fs.o \
  $(BK)/log.o \
  $(BK)/sleeplock.o \
  $(BK)/file.o \
  $(BK)/pipe.o \
  $(BK)/exec.o \
  $(BK)/sysfile.o \
  $(BK)/kernelvec.o \
  $(BK)/plic.o \
  $(BK)/virtio_disk.o

# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-elf-'; \
	elif riscv64-none-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-none-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64
MIN_QEMU_VERSION = 7.2

CC = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -Wno-unknown-attributes -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -march=rv64gc
CFLAGS += -std=gnu99
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding
CFLAGS += -fno-common -nostdlib
CFLAGS += -fno-builtin-strncpy -fno-builtin-strncmp -fno-builtin-strlen -fno-builtin-memset
CFLAGS += -fno-builtin-memmove -fno-builtin-memcmp -fno-builtin-log -fno-builtin-bzero
CFLAGS += -fno-builtin-strchr -fno-builtin-exit -fno-builtin-malloc -fno-builtin-putc
CFLAGS += -fno-builtin-free
CFLAGS += -fno-builtin-memcpy -Wno-main
CFLAGS += -fno-builtin-printf -fno-builtin-fprintf -fno-builtin-vprintf
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096
DEPFLAGS = -MMD -MP -MF $(@:.o=.d)

all: $(KERNEL) $(FSIMG)

$(B) $(BK) $(BU) $(BM):
	mkdir -p $@

$(KERNEL): $(OBJS) $(K)/kernel.ld | $(BK)
	$(LD) $(LDFLAGS) -T $(K)/kernel.ld -o $@ $(OBJS)
	$(OBJDUMP) -S $@ > $(BK)/kernel.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(BK)/kernel.sym

$(BK)/%.o: $(K)/%.c | $(BK)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BK)/%.o: $(K)/%.S | $(BK)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

tags: $(OBJS)
	etags kernel/*.S kernel/*.c

ULIB = $(BU)/ulib.o $(BU)/usys.o $(BU)/printf.o $(BU)/umalloc.o

$(BU)/_%: $(BU)/%.o $(ULIB) $(U)/user.ld | $(BU)
	$(LD) $(LDFLAGS) -T $(U)/user.ld -o $@ $< $(ULIB)
	$(OBJDUMP) -S $@ > $(BU)/$*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(BU)/$*.sym

$(BU)/%.o: $(U)/%.c | $(BU)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BU)/%.o: $(U)/%.S | $(BU)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BU)/usys.S: $(U)/usys.pl | $(BU)
	perl $< > $@

$(BU)/usys.o: $(BU)/usys.S | $(BU)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BU)/_forktest: $(BU)/forktest.o $(BU)/ulib.o $(BU)/usys.o | $(BU)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $(BU)/forktest.o $(BU)/ulib.o $(BU)/usys.o
	$(OBJDUMP) -S $@ > $(BU)/forktest.asm

$(BM)/mkfs: $(M)/mkfs.c $(K)/fs.h $(K)/param.h | $(BM)
	gcc -Wno-unknown-attributes -I. -o $@ $(M)/mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: $(BK)/%.o $(BU)/%.o

UPROGS=\
	$(BU)/_cat\
	$(BU)/_echo\
	$(BU)/_forktest\
	$(BU)/_grep\
	$(BU)/_init\
	$(BU)/_kill\
	$(BU)/_ln\
	$(BU)/_ls\
	$(BU)/_mkdir\
	$(BU)/_rm\
	$(BU)/_sh\
	$(BU)/_stressfs\
	$(BU)/_usertests\
	$(BU)/_grind\
	$(BU)/_wc\
	$(BU)/_zombie\
	$(BU)/_logstress\
	$(BU)/_forphan\
	$(BU)/_dorphan\
	$(BU)/_pipetest\
	$(BU)/_ps\
	$(BU)/_fcfstest\
	$(BU)/_mlfqtest\
	$(BU)/_csw\
	$(BU)/_throughput\
	$(BU)/_halt\
	$(BU)/_lseektest\
	$(BU)/_semtest1\
	$(BU)/_semtest2\
	$(BU)/_semtest3\
	$(BU)/_waitpidtest

$(FSIMG): $(BM)/mkfs README $(UPROGS) | $(B)
	$(BM)/mkfs $@ README $(UPROGS)

-include $(BK)/*.d $(BU)/*.d

clean: 
	rm -rf $(B)
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg .gdbinit
	rm -f $(K)/*.o $(K)/*.d $(K)/*.asm $(K)/*.sym $(K)/kernel
	rm -f $(U)/*.o $(U)/*.d $(U)/*.asm $(U)/*.sym $(U)/_*
	rm -f $(M)/mkfs fs.img

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 3
endif

QEMUOPTS = -machine virt -bios none -kernel $(KERNEL) -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=$(FSIMG),if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

qemu: check-qemu-version $(KERNEL) $(FSIMG)
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $(KERNEL) .gdbinit $(FSIMG)
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

print-gdbport:
	@echo $(GDBPORT)

QEMU_VERSION := $(shell $(QEMU) --version | head -n 1 | sed -E 's/^QEMU emulator version ([0-9]+\.[0-9]+)\..*/\1/')
check-qemu-version:
	@if [ "$(shell echo "$(QEMU_VERSION) >= $(MIN_QEMU_VERSION)" | bc)" -eq 0 ]; then \
		echo "ERROR: Need qemu version >= $(MIN_QEMU_VERSION)"; \
		exit 1; \
	fi

.PHONY: all clean fmt qemu qemu-gdb check-qemu-version print-gdbport tags
fmt:
	clang-format -i $(wildcard kernel/*.[ch] user/*.[ch] mkfs/*.c)
