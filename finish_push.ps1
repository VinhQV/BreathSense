# Auto-resolve remaining conflicts (take local version) and push to GitHub
$ErrorActionPreference = "Stop"

Write-Host "Resolving remaining conflicts (keeping local version)..." -ForegroundColor Cyan
git checkout --theirs `
    app.c `
    "autogen/sbom/cyclonedx_bom.json" `
    "autogen/sbom/cyclonedx_bom.xml" `
    "autogen/sbom/spdx_bom.spdx" `
    "autogen/sbom/spdx_bom.spdx.json" `
    "autogen/sl_component_catalog.h" `
    "autogen/sl_simple_button_instances.c" `
    "autogen/sl_simple_button_instances.h" `
    "breathsense_mg21.pintool" `
    "breathsense_mg21.slcp" `
    "cmake_gcc/breathsense_mg21.cmake"

Write-Host "Staging all files..." -ForegroundColor Cyan
git add -A

Write-Host "Continuing rebase..." -ForegroundColor Cyan
$env:GIT_EDITOR = "true"
git rebase --continue

Write-Host "Pushing to GitHub..." -ForegroundColor Cyan
git push --set-upstream origin main

Write-Host "Done!" -ForegroundColor Green
