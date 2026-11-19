#!/usr/bin/env python3
"""Генерує src/Favicon.h з PNG-файлу (за замовчуванням icons/hexbrow_32.png).

Запуск з кореня репозиторію:
    python3 tools/make_favicon_header.py
"""
import base64
import pathlib
import sys

src = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "icons/hexbrow_32.png")
out = pathlib.Path("src/Favicon.h")

data = base64.b64encode(src.read_bytes()).decode("ascii")
out.write_text(
    "// Згенеровано tools/make_favicon_header.py. Не редагувати вручну.\n"
    "#pragma once\n"
    "#include <string>\n\n"
    "// Favicon у вигляді data URI (PNG 32x32, base64)\n"
    f'static const std::string FAVICON_DATA_URI = "data:image/png;base64,{data}";\n',
    encoding="utf-8",
)
print(f"written {out} ({out.stat().st_size} bytes) from {src}")
