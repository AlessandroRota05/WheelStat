"""Prints the QR code of the text in a file as rows of 0/1, first line the
size. Used by preview.cpp in place of the ESP32 core's encoder: same
standard, lowest error correction raised when it fits (like the core's)."""
import sys

import segno

with open(sys.argv[1], encoding="utf-8") as f:
    text = f.read()
code = segno.make(text, error="l", micro=False)
print(len(code.matrix))
for row in code.matrix:
    print("".join("1" if m else "0" for m in row))
