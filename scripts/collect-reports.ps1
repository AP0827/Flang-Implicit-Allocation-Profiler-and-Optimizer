param(
  [string]$InputDir = "examples",
  [string]$OutputDir = "out",
  [ValidateSet("text", "json", "dot")]
  [string]$Format = "json"
)

$tool = "build/fiap-opt.exe"
if (-not (Test-Path -LiteralPath $tool)) {
  Write-Error "fiap-opt has not been built yet."
  exit 1
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Get-ChildItem -LiteralPath $InputDir -Filter *.mlir | ForEach-Object {
  $outFile = Join-Path $OutputDir ($_.BaseName + "." + $Format)
  & $tool $_.FullName "--format=$Format" | Set-Content -LiteralPath $outFile
  Write-Host "wrote $outFile"
}
