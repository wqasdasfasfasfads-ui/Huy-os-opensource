#!/bin/bash
# Збірка Huy OS. Потрібно: nasm gcc binutils python3 (qemu-utils - необов'язково, для .vdi/.vmdk)
set -e
cd "$(dirname "$0")"
mkdir -p build

# пакуємо files/ у build/files.bin (вбудовується в ядро через .incbin):
# [ім'я 32][розмір u32][0 u32][дані, вирівняно до 8]...[порожній заголовок]
python3 - <<'PY'
import os, struct
out = b''
for n in sorted(os.listdir('files')):
    d = open(os.path.join('files', n), 'rb').read()
    out += n.encode()[:31].ljust(32, b'\0') + struct.pack('<II', len(d), 0) + d + b'\0' * (-len(d) % 8)
open('build/files.bin', 'wb').write(out + b'\0' * 40)
PY

nasm -f elf64 -I build/ src/entry.asm -o build/entry.o
gcc -m64 -ffreestanding -fno-pic -fno-pie -mcmodel=small -mno-red-zone -mno-sse -mno-mmx -mno-sse2 \
    -fno-stack-protector -fno-builtin -fno-asynchronous-unwind-tables -Os -Wall -Wno-unused-function \
    -c src/kernel.c -o build/kernel.o
ld -m elf_x86_64 -z max-page-size=0x1000 -T src/linker.ld -nostdlib build/entry.o build/kernel.o -o build/kernel.elf
objcopy -O binary build/kernel.elf build/kernel.bin
SIZE=$(stat -c %s build/kernel.bin)
KS=$(( (SIZE + 511) / 512 ))
[ $KS -le 120 ] || { echo "ядро завелике ($KS секторів)"; exit 1; }

nasm -f bin -d KSECTORS=$KS src/boot.asm -o build/boot.bin
dd if=/dev/zero of=build/huyos.img bs=1M count=16 status=none
dd if=build/boot.bin of=build/huyos.img conv=notrunc status=none
dd if=build/kernel.bin of=build/huyos.img bs=512 seek=1 conv=notrunc status=none
if command -v qemu-img >/dev/null; then
  qemu-img convert -f raw -O vdi  build/huyos.img build/huyos.vdi
  qemu-img convert -f raw -O vmdk build/huyos.img build/huyos.vmdk
fi

# ISO (El Torito, no-emulation) - власний скрипт, xorriso не потрібен
nasm -f bin -d KSECTORS=$KS -d ISO src/boot.asm -o build/boot_iso.bin
python3 tools/mkiso.py build/huyos.iso build/boot_iso.bin build/kernel.bin $KS
echo "OK: build/huyos.img + build/huyos.iso (64-bit, ядро $SIZE байт, $KS секторів)"
