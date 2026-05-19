param(
  [int]$Port = 5173,
  [switch]$NoInstall
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$ui = Join-Path $root "ui"

if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
  throw "Node.js was not found on PATH. Install Node.js 20 or newer, then rerun this script."
}
if (-not (Get-Command npm -ErrorAction SilentlyContinue)) {
  throw "npm was not found on PATH. Install Node.js with npm, then rerun this script."
}

Push-Location $ui
try {
  if (-not $NoInstall -and -not (Test-Path -LiteralPath "node_modules")) {
    npm install
  }

  $env:PORT = "$Port"
  npm run dev
}
finally {
  Pop-Location
}
