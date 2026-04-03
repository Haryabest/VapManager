#!/usr/bin/env python3
"""Конвертирует noback/agvIcon.png в noback/agvIcon.ico для иконки exe."""
import os
import struct

png_path = os.path.join(os.path.dirname(__file__), "noback", "agvIcon.png")
ico_path = os.path.join(os.path.dirname(__file__), "noback", "agvIcon.ico")

with open(png_path, "rb") as f:
    png_data = f.read()

header = struct.pack("<HHH", 0, 1, 1)
offset = 6 + 16
entry = struct.pack("<BBBBHHII", 0, 0, 0, 0, 1, 32, len(png_data), offset)

with open(ico_path, "wb") as f:
    f.write(header)
    f.write(entry)
    f.write(png_data)

print(f"Создан: {ico_path}")
