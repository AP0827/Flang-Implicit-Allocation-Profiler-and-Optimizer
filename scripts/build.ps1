param(
  [string]$BuildDir = "build",
  [string]$LLVM_DIR = "D:\llvm-project\build\lib\cmake\llvm",
  [string]$MLIR_DIR = "D:\llvm-project\build\lib\cmake\mlir",
  [string]$Flang_DIR = "D:\llvm-project\build\lib\cmake\flang",
  [string]$Generator = "NMake Makefiles"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$vsDevCmd = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (-not (Test-Path -LiteralPath $vsDevCmd)) {
  throw "Visual Studio developer command prompt was not found: $vsDevCmd"
}
if (-not (Test-Path -LiteralPath $cmake)) {
  throw "CMake was not found: $cmake"
}

$configure = @(
  "call `"$vsDevCmd`" -arch=x64",
  "&&",
  "`"$cmake`"",
  "-G `"$Generator`"",
  "-S `"$root`"",
  "-B `"$BuildDir`"",
  "-DCMAKE_BUILD_TYPE=Release",
  "-DLLVM_DIR=`"$LLVM_DIR`"",
  "-DMLIR_DIR=`"$MLIR_DIR`""
)

if (Test-Path -LiteralPath $Flang_DIR) {
  $configure += "-DFlang_DIR=`"$Flang_DIR`""
}

$build = @(
  "&&",
  "`"$cmake`"",
  "--build `"$BuildDir`"",
  "--config Release"
)

cmd /c ($configure + $build -join " ")
