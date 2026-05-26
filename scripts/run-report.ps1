param(
  [string]$Input = "examples/implicit_temp.mlir",
  [ValidateSet("text", "json", "dot")]
  [string]$Format = "text",
  [switch]$PrintAnnotatedIR
)

$tool = "build/fiap-opt.exe"
if (-not (Test-Path -LiteralPath $tool)) {
  Write-Error "fiap-opt has not been built yet."
  exit 1
}

$args = @($Input, "--format=$Format")
if ($PrintAnnotatedIR) {
  $args += "--print-annotated-ir"
}

& $tool @args
