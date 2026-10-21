#!/usr/bin/env bash
set -e
export N64_INST=/opt/libdragon
export PATH=/opt/libdragon/bin:$PATH
ln -sfn "/mnt/c/Desenvolvimento/Nintendo 64/GTA2 N64 Tech Demo" /home/georges/gta2_n64
cd /home/georges/gta2_n64
make "$@"
