#!/usr/bin/env bash
# Atalho de build: bash build.sh            -> gera gta2_recomp.z64
#                  bash build.sh clean      -> limpa
# Requer libdragon (toolchain MIPS + n64.mk). Caminho padrao: /opt/libdragon (mude com N64_INST=...).
set -e
cd "$(dirname "$0")"
export N64_INST="${N64_INST:-/opt/libdragon}"
export PATH="$N64_INST/bin:$PATH"
[ -d port/gta2_re/Source ] || bash ./setup_engine.sh
if [ "${1:-}" = "clean" ]; then exec make -f Makefile.recomp clean; fi
exec make -f Makefile.recomp -j"$(nproc)" all "$@"
