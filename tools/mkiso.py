#!/usr/bin/env python3
# Мінімальний ISO9660 + El Torito (no-emulation, x86 BIOS). Використання: mkiso.py out.iso boot_iso.bin kernel.bin KSECTORS
import struct, sys, datetime
out, bootf, kern, ks = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
S = 2048
img = open(bootf, 'rb').read()
assert len(img) == 512
img += open(kern, 'rb').read().ljust(ks * 512, b'\0')
vsect = ks + 1                                  # скільки 512-байтних секторів вантажить BIOS
img = img.ljust((len(img) + S - 1) // S * S, b'\0')
BOOT_LBA, nblocks = 23, 23 + len(img) // S
def b2(v): return struct.pack('<H', v) + struct.pack('>H', v)
def b4(v): return struct.pack('<I', v) + struct.pack('>I', v)
now = datetime.datetime.now(datetime.timezone.utc)
d7 = bytes([now.year - 1900, now.month, now.day, now.hour, now.minute, now.second, 0])
d17 = now.strftime('%Y%m%d%H%M%S').encode() + b'00\0'
def rec(name, lba, size, flags):
    n = len(name) + 33
    n += n & 1
    r = bytes([n, 0]) + b4(lba) + b4(size) + d7 + bytes([flags, 0, 0]) + b2(1) + bytes([len(name)]) + name
    return r.ljust(n, b'\0')
def pad(b): return b.ljust(S, b'\0')
root = rec(b'\0', 22, S, 2) + rec(b'\1', 22, S, 2) + rec(b'HUYOS.BIN;1', BOOT_LBA, len(img), 0)
ptl = struct.pack('<BBIH', 1, 0, 22, 1) + b'\0\0'
ptm = struct.pack('>BBIH', 1, 0, 22, 1) + b'\0\0'
def sp(s, n): return s.encode().ljust(n, b' ')
pvd = b'\x01CD001\x01\0' + sp('HUYOS', 32) + sp('HUYOS', 32) + b'\0' * 8 + b4(nblocks) + b'\0' * 32 + b2(1) + b2(1) + b2(S) + b4(10)
pvd += struct.pack('<I', 20) + struct.pack('<I', 0) + struct.pack('>I', 21) + struct.pack('>I', 0)
pvd += rec(b'\0', 22, S, 2)[:34] + sp('', 128 * 4) + sp('', 37 * 3) + d17 * 2 + b'0000000000000000\0' + b'0000000000000000\0' + b'\x01'
pvd = pad(pvd)
br = pad(b'\0CD001\x01' + b'EL TORITO SPECIFICATION'.ljust(32, b'\0') + b'\0' * 32 + struct.pack('<I', 19))
term = pad(b'\xffCD001\x01')
val = bytearray(b'\x01\x00\0\0' + b'HUYOS'.ljust(24, b'\0') + b'\0\0\x55\xaa')
val[28:30] = struct.pack('<H', (-sum(struct.unpack('<16H', bytes(val[:28]) + b'\0\0' + b'\x55\xaa')) ) & 0xFFFF)
cat = pad(bytes(val) + struct.pack('<BBHBBHI', 0x88, 0, 0, 0, 0, vsect, BOOT_LBA) + b'\0' * 20)
iso = b'\0' * (16 * S) + pvd + br + term + cat + pad(ptl) + pad(ptm) + pad(root) + img
open(out, 'wb').write(iso)
print('ISO:', out, len(iso), 'байт')
