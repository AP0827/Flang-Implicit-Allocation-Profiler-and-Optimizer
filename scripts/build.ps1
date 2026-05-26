param(
  [string]$BuildDir = "build",
  [string]$LLVM_DIR = "D:\llvm-project\build\lib\cmake\llvm",
  [string]$MLIR_DIR = "D:\llvm-project\build\lib\cmake\mlir",
  [string]$Flang_DIR = "D:\llvm-project\build\lib\cmake\flang",
  [string]$Generator = "NMake Makefiles",
  [switch]$VerboseLog
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $root $BuildDir
$logDir = Join-Path $buildPath "logs"
$logFile = Join-Path $logDir "build.log"
$vsDevCmd = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

function Write-Step($Message) {
  Write-Host $Message -ForegroundColor Cyan
}

function Invoke-LoggedCmd($Label, $Command) {
  Write-Step $Label
  $wrapped = "call `"$vsDevCmd`" -arch=x64 >NUL && $Command"
  $output = cmd /c $wrapped 2>&1
  $exit = $LASTEXITCODE
  $output | Add-Content -LiteralPath $logFile
  if ($VerboseLog -and $output) {
    $output | Write-Host
  }
  if ($exit -ne 0) {
    Write-Host "failed. See $logFile" -ForegroundColor Red
    exit $exit
  }
  Write-Host "ok" -ForegroundColor Green
}

if (-not (Test-Path -LiteralPath $vsDevCmd)) {
  throw "Visual Studio developer command prompt was not found: $vsDevCmd"
}
if (-not (Test-Path -LiteralPath $cmake)) {
  throw "CMake was not found: $cmake"
}

New-Item -ItemType Directory -Force -Path $logDir | Out-Null
"FIAP build log - $(Get-Date -Format s)" | Set-Content -LiteralPath $logFile

Write-Host ""
Write-Host "FIAP build" -ForegroundColor White
Write-Host "log: $logFile" -ForegroundColor DarkGray

$cmakeArgs = @(
  "-G `"$Generator`"",
  "-S `"$root`"",
  "-B `"$buildPath`"",
  "-DCMAKE_BUILD_TYPE=Release",
  "-DLLVM_DIR=`"$LLVM_DIR`"",
  "-DMLIR_DIR=`"$MLIR_DIR`""
)

if (Test-Path -LiteralPath $Flang_DIR) {
  $cmakeArgs += "-DFlang_DIR=`"$Flang_DIR`""
}

Invoke-LoggedCmd "[1/2] Configuring CMake" "`"$cmake`" $($cmakeArgs -join ' ')"
Invoke-LoggedCmd "[2/2] Building fiap-opt" "`"$cmake`" --build `"$buildPath`" --config Release"

$tool = Join-Path $buildPath "fiap-opt.exe"
if (-not (Test-Path -LiteralPath $tool)) {
  $tool = Join-Path $buildPath "Release\fiap-opt.exe"
}

Write-Host ""
Write-Host "Build complete" -ForegroundColor Green
Write-Host "tool: $tool"
