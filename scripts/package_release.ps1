param(
  [string]$Version = "1.0.0",
  [string]$OutputDir = "out"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$stage = Join-Path $root "$OutputDir\fiap-$Version"
$zip = Join-Path $root "$OutputDir\fiap-$Version.zip"

if (Test-Path -LiteralPath $stage) {
  Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null

$items = @(
  "README.md",
  "DESIGN.md",
  "IMPLEMENTATION.md",
  "EVALUATION.md",
  "CMakeLists.txt",
  "cmake",
  "include",
  "lib",
  "tools",
  "src",
  "scripts",
  "testcases",
  "docs",
  "profiles",
  "benchmarks",
  "upstream",
  ".github"
)

foreach ($item in $items) {
  $source = Join-Path $root $item
  if (Test-Path -LiteralPath $source) {
    Copy-Item -LiteralPath $source -Destination $stage -Recurse -Force
  }
}

Get-ChildItem -Path $stage -Recurse -Directory -Filter "__pycache__" |
  Remove-Item -Recurse -Force

if (Test-Path -LiteralPath $zip) {
  Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip

Write-Host "release package: $zip"
