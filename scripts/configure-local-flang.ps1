param(
  [Parameter(Mandatory = $true)]
  [string]$LlvmBuildDir,
  [string]$ProjectDir = "D:\Flang-Implicit-Allocation-Profiler-and-Optimizer",
  [string]$BuildDir = "D:\Flang-Implicit-Allocation-Profiler-and-Optimizer\build"
)

$llvmDir = Join-Path $LlvmBuildDir "lib\cmake\llvm"
$mlirDir = Join-Path $LlvmBuildDir "lib\cmake\mlir"
$flangDir = Join-Path $LlvmBuildDir "lib\cmake\flang"

foreach ($path in @($llvmDir, $mlirDir, $flangDir)) {
  if (-not (Test-Path -LiteralPath $path)) {
    Write-Error "Missing expected CMake package directory: $path"
    exit 1
  }
}

cmake -S $ProjectDir -B $BuildDir `
  -DLLVM_DIR=$llvmDir `
  -DMLIR_DIR=$mlirDir `
  -DFlang_DIR=$flangDir
