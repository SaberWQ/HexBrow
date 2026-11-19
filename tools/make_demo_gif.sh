#!/usr/bin/env bash
# Перетворює запис екрана (.mov) на стиснений GIF для README.
# Використання:  tools/make_demo_gif.sh demo.mov [docs/screenshots/demo.gif]
# Потрібно: ffmpeg (brew install ffmpeg)
set -euo pipefail

IN="${1:?Вкажіть вхідний файл, наприклад demo.mov}"
OUT="${2:-docs/screenshots/demo.gif}"
FPS=12          # 10–12 достатньо для демонстрації інтерфейсу
WIDTH=960       # ширина в пікселях; висота пропорційна
MAX_MB=8        # GitHub показує великі GIF, але краще не більше 8 МБ

command -v ffmpeg >/dev/null || { echo "Немає ffmpeg: brew install ffmpeg"; exit 1; }
mkdir -p "$(dirname "$OUT")"
TMP_PALETTE="$(mktemp -t palette).png"

# Двопрохідне перетворення: окрема палітра дає чіткіші кольори й менший розмір
ffmpeg -y -loglevel error -i "$IN" \
  -vf "fps=${FPS},scale=${WIDTH}:-1:flags=lanczos,palettegen=stats_mode=diff" \
  "$TMP_PALETTE"
ffmpeg -y -loglevel error -i "$IN" -i "$TMP_PALETTE" \
  -lavfi "fps=${FPS},scale=${WIDTH}:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer" \
  -loop 0 "$OUT"
rm -f "$TMP_PALETTE"

SIZE_MB=$(( $(wc -c < "$OUT") / 1048576 ))
echo "Готово: $OUT (${SIZE_MB} МБ)"
if [ "$SIZE_MB" -gt "$MAX_MB" ]; then
  echo "Увага: файл більший за ${MAX_MB} МБ. Зменште WIDTH або FPS у скрипті."
fi
