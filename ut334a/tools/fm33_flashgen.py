"""Turn a flash image (loaded at 0x0) into an fm33_flash.tcl erase/program script and the expected readback.

Usage: fm33_flashgen.py image.bin out.tcl expected.bin
Erases all 512 pages of the 256 KB main array page by page (the stock firmware fills almost all of it, and
Rad Pro keeps settings and datalog above the firmware slot), then programs every word that is not 0xFFFFFFFF.
expected.bin is the image padded with 0xFF to 256 KB, for comparing with the readback.
"""
import struct
import sys

FLASH_SIZE = 0x40000
PAGE = 0x200

img = open(sys.argv[1], 'rb').read()
if len(img) > FLASH_SIZE:
    sys.exit(f'image too large: {len(img)} bytes')
img = img + b'\xff' * (FLASH_SIZE - len(img))
sp, reset = struct.unpack('<II', img[:8])
if not (0x20000000 < sp <= 0x20006000) or not (reset & 1) or reset >= FLASH_SIZE:
    sys.exit(f'image does not start with a valid vector table (SP 0x{sp:08X}, reset 0x{reset:08X})')

out = ['fm33_flash_begin', 'echo ">>> Erasing 512 pages..."']
for p in range(0, FLASH_SIZE, PAGE):
    out.append(f'fm33_erase_page 0x{p:08X}')
    if (p // PAGE) % 64 == 63:
        out.append(f'echo "    erased up to 0x{p + PAGE:05X}"')

words = struct.unpack(f'<{FLASH_SIZE // 4}I', img)
n = sum(1 for w in words if w != 0xFFFFFFFF)
out.append(f'echo ">>> Programming {n} words..."')
done = 0
i = 0
while i < len(words):
    if words[i] == 0xFFFFFFFF:
        i += 1
        continue
    j = i
    while j < len(words) and words[j] != 0xFFFFFFFF and j - i < 64:
        j += 1
    out.append(f'fm33_program_words 0x{4 * i:08X} {{' + ' '.join(f'0x{w:08X}' for w in words[i:j]) + '}')
    done += j - i
    if done // 4096 != (done - (j - i)) // 4096:
        out.append(f'echo "    {done}/{n} words"')
    i = j
out.append('fm33_flash_end')
out.append('echo ">>> Erase/program done."')
open(sys.argv[2], 'w', newline='\n').write('\n'.join(out) + '\n')
open(sys.argv[3], 'wb').write(img)
print(f'{n} words to program; script {sys.argv[2]}')
