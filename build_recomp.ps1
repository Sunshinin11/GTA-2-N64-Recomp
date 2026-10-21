# Compila a ROM de teste do motor gta2_re (renderer nulo) no WSL e guarda o log.
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location -LiteralPath $project
try {
    Write-Host "Compilando gta2_recomp.z64 no WSL (leva alguns minutos)..."
    & wsl bash -lc 'set -o pipefail; cd /home/georges/gta2_n64 && bash ./build.sh -f Makefile.recomp -j4 all 2>&1 | tee build_recomp_log.txt'
    if ($LASTEXITCODE -eq 0) { Write-Host "BUILD OK: gta2_recomp.z64 atualizada." }
    else { Write-Host "BUILD FALHOU (codigo $LASTEXITCODE): a ROM NAO foi atualizada. Veja as linhas com 'error' acima ou envie build_recomp_log.txt." }
} finally { Pop-Location }
