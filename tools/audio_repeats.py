# Counts audio buffers in an INDY64_AUDIO_RAW recording that repeat an earlier buffer exactly (stutter: the game
# handed over a buffer before the microcode refilled it). Near-silent buffers (peak below 64) are reported apart,
# because a constant level repeats legitimately.
# Usage: python -I tools/audio_repeats.py <raw file> [frames_per_buffer]
import array
import sys

data = open(sys.argv[1], "rb").read()
frames = int(sys.argv[2]) if len(sys.argv) > 2 else 192
size = frames * 4
buffers = [data[i:i + size] for i in range(0, len(data) - size + 1, size)]

recent = []
loud = quiet = 0
loud_at = []
for index, buf in enumerate(buffers):
    if buf in recent:
        samples = array.array("h")
        samples.frombytes(buf)
        peak = max(abs(s) for s in samples)
        if peak < 64:
            quiet += 1
        else:
            loud += 1
            loud_at.append(index)
    recent.append(buf)
    if len(recent) > 16:
        recent.pop(0)

print(f"{len(buffers)} buffers; repeats: {loud} audible ({100 * loud / max(1, len(buffers)):.2f} %), {quiet} near-silent")
# Audible repeats per 10-second block (at 22050 Hz, 115 buffers per second).
blocks = {}
for i in loud_at:
    blocks[i // 1150] = blocks.get(i // 1150, 0) + 1
print("audible repeats per 10 s block:", dict(sorted(blocks.items())))
