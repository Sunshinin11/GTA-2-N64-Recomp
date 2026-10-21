#!/usr/bin/env bash
# Copia dados do GTA 2 (freeware) para o DragonFS com nomes em minusculas.
# Arquivos .gxt/.sty/.gmp sao convertidos de little-endian (PC) para big-endian (N64) por tools/le2be.py,
# SEMPRE a partir do original (nunca converte in-place -> sem risco de troca dupla).
# uso: stage_data.sh <pasta_data_do_jogo> <pasta_fs_saida> <entradas...>   (entrada = arquivo ou pasta, sem diferenciar maiusculas)
src="$1"; out="$2"; shift 2
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$out/data"
for e in "$@"; do
  case "$e" in
    */*) m=$(find "$src" -maxdepth 2 -ipath "$src/$e" | head -1) ;;   # ex.: audio/bil.raw
    *)   m=$(find "$src" -maxdepth 1 -iname "$e" | head -1) ;;
  esac
  if [ -z "$m" ]; then echo "AVISO: '$e' nao existe em $src"; continue; fi
  while read -r f; do
    rel="${f#$src/}"; low=$(echo "$rel" | tr 'A-Z' 'a-z')
    mkdir -p "$out/data/$(dirname "$low")"
    case "$low" in
      old\ *|*/old\ *) continue ;;
      *.gxt|*.sty|*.gmp|*.tga)
        python3 "$here/le2be.py" "$f" "$out/data/$low" || { echo "ERRO convertendo $f"; exit 1; } ;;
      gtaudio/*.wav)
        dst="$out/data/${low%.wav}.wav64"
        if [ ! -f "$dst" ] || [ "$f" -nt "$dst" ]; then
          echo "    [RADIO] $low (ULC mono 16 kHz)"
          "${N64_INST:-/opt/libdragon}/bin/audioconv64" --wav-compress ulc,quality=35 --wav-mono --wav-resample 16000 \
            -o "$out/data/gtaudio" "$f" >/dev/null || { echo "ERRO convertendo $f"; exit 1; }
          [ -f "$dst" ] || mv "$out/data/gtaudio/$(basename "${f%.*}").wav64" "$dst" 2>/dev/null
        fi ;;
      audio/*.raw|audio/*.sdt)
        python3 "$here/audio_le2be.py" "$f" "$out/data/$low" || { echo "ERRO convertendo $f"; exit 1; } ;;
      *) cp -u "$f" "$out/data/$low" ;;
    esac
  done < <(find "$m" -type f)
done
