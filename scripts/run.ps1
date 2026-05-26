param(
  [string]$InputFile = "",
  [ValidateSet("text", "json", "dot")]
  [string]$Format = "text",
  [switch]$PrepareTransforms,
  [switch]$PrintAnnotatedIR,
  [string]$BuildDir = "build",
  [string]$ReportsDir = "reports",
  [string]$Flang = "",
  [string]$Compiler = "",
  [int]$BenchmarkRuns = 5,
  [switch]$SkipBenchmark,
  [switch]$IncludeMlirRegression,
  [switch]$IncludeCTest
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$tool = Join-Path $root "$BuildDir\fiap-opt.exe"
if (-not (Test-Path -LiteralPath $tool)) {
  $tool = Join-Path $root "$BuildDir\Release\fiap-opt.exe"
}

if (-not (Test-Path -LiteralPath $tool)) {
  throw "fiap-opt was not found. Run scripts\build.ps1 first."
}

if ($InputFile -and $InputFile.ToLowerInvariant().EndsWith(".mlir")) {
  $toolArgs = @($InputFile, "--format=$Format")
  if ($PrepareTransforms) {
    $toolArgs += "--prepare-transforms"
  }
  if ($PrintAnnotatedIR) {
    $toolArgs += "--print-annotated-ir"
  }
  & $tool @toolArgs
  exit $LASTEXITCODE
}

$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $python) {
  $python = (Get-Command py -ErrorAction SilentlyContinue).Source
}
if (-not $python) {
  throw "Python was not found on PATH."
}

$demoArgs = @(
  (Join-Path $root "scripts\run_backend_demo.py"),
  "--tool", $tool,
  "--build-dir", (Join-Path $root $BuildDir),
  "--reports-dir", (Join-Path $root $ReportsDir),
  "--benchmark-runs", "$BenchmarkRuns"
)
if ($Flang) {
  $demoArgs += @("--flang", $Flang)
}
if ($Compiler) {
  $demoArgs += @("--compiler", $Compiler)
}
if ($SkipBenchmark) {
  $demoArgs += "--skip-benchmark"
}
if ($IncludeMlirRegression) {
  $demoArgs += "--include-mlir-regression"
}
if ($IncludeCTest) {
  $demoArgs += "--include-ctest"
}

& $python @demoArgs
