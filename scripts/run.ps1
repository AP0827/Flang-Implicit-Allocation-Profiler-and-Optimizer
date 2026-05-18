param(
  [string]$InputFile = "testcases\01_array_temp.mlir",
  [ValidateSet("text", "json", "dot")]
  [string]$Format = "text",
  [switch]$PrepareTransforms,
  [switch]$PrintAnnotatedIR
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$tool = Join-Path $root "build\fiap-opt.exe"

if (-not (Test-Path -LiteralPath $tool)) {
  throw "fiap-opt was not found. Run scripts\build.ps1 first."
}

$toolArgs = @($InputFile, "--format=$Format")
if ($PrepareTransforms) {
  $toolArgs += "--prepare-transforms"
}
if ($PrintAnnotatedIR) {
  $toolArgs += "--print-annotated-ir"
}

& $tool @toolArgs
