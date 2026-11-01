# Summarises a raw recording from INDY64_AUDIO_RAW (16-bit stereo) in blocks: loudness, silence, clipping, and
# writes a WAV copy at the given rate for listening.
# Usage: python -I tools/audio_stats.py <raw file> [rate] [block_frames]
import array
import math
import sys
import wave

path = sys.argv[1]
rate = int(sys.argv[2]) if len(sys.argv) > 2 else 22050
block = int(sys.argv[3]) if len(sys.argv) > 3 else rate

data = array.array("h")
with open(path, "rb") as f:
    data.frombytes(f.read())
frames = len(data) // 2
print(f"{frames} frames = {frames / rate:.1f} s at {rate} Hz")

for start in range(0, frames, block):
    chunk = data[start * 2:(start + block) * 2]
    n = len(chunk)
    rms = math.sqrt(sum(s * s for s in chunk) / n) if n else 0
    zeros = sum(1 for s in chunk if s == 0) / n
    clip = sum(1 for s in chunk if abs(s) >= 32767) / n
    peak = max(abs(s) for s in chunk)
    left = chunk[0::2]
    crossings = sum(1 for i in range(1, len(left)) if (left[i - 1] < 0) != (left[i] < 0))
    print(f"{start / rate:6.1f}s  rms {rms:7.0f}  peak {peak:5d}  zero {zeros:4.0%}  clip {clip:5.1%}  crossings {crossings}")

with wave.open(path.rsplit(".", 1)[0] + ".wav", "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(data.tobytes())
