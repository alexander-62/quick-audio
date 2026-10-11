"""Generate the original QuickAudio speaker icon, using only the standard library."""
from pathlib import Path
import struct

size = 32
pixels = bytearray()
for y in reversed(range(size)):
    for x in range(size):
        inside = 2 <= x <= 29 and 2 <= y <= 29
        speaker = (7 <= x <= 11 and 13 <= y <= 19) or (11 <= x <= 18 and abs(y - 16) <= x - 8)
        waves = (21 <= x <= 23 and 11 <= y <= 21 and (x == 23 or y in (11, 21)))
        color = (255, 255, 255, 255) if speaker or waves else ((228, 142, 36, 255) if inside else (0, 0, 0, 0))
        pixels.extend(color)
mask = bytes(4 * size)
dib = struct.pack('<IIIHHIIIIII', 40, size, size * 2, 1, 32, 0, len(pixels), 0, 0, 0, 0) + pixels + mask
header = struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', size, size, 0, 0, 1, 32, len(dib), 22)
Path(__file__).resolve().parents[1].joinpath('src/app.ico').write_bytes(header + dib)
