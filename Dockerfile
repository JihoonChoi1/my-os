# ============================================================
# Stage 1: Build the OS with system gcc (Ubuntu, no cross-compiler needed)
#
# We override CC and LD to use the system toolchain.
# gcc-multilib provides -m32 support on x86_64.
# ============================================================
# Use x86_64 Ubuntu explicitly.
# On Apple Silicon Macs, Docker defaults to ARM64 where gcc-multilib
# is unavailable. --platform=linux/amd64 forces x86_64 (via Rosetta).
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
  gcc \
  gcc-multilib \
  nasm \
  make \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /os
COPY . .

# Override CC/LD: system gcc instead of x86_64-elf-gcc
# -fno-stack-protector: disable stack canary (no __stack_chk_fail in bare metal)
# Use 'disk.img' target directly — default target is 'run' which starts QEMU
# Include paths are hardcoded because $(foreach) is Makefile syntax, not shell
RUN make disk.img CC=gcc LD=ld \
  CFLAGS="-ffreestanding -m32 -g -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx \
  -Ikernel -Icpu -Idrivers -Imm -Ifs"

# ============================================================
# Stage 2: Minimal runtime — just QEMU + disk.img (~200MB)
# ============================================================
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
  qemu-system-x86 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /os
COPY --from=builder /os/disk.img .

CMD ["qemu-system-x86_64", \
  "-no-shutdown", \
  "-display", "none", \
  "-serial", "stdio", \
  "-drive", "format=raw,file=disk.img"]
