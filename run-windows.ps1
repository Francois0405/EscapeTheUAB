# EscapeTheUAB - compilar y ejecutar en Windows con una sola orden.
# En la raiz del repositorio, al lado de CMakePresets.json:
#   .\run-windows.cmd
#   .\run-windows.cmd -Configuration Release
#   .\run-windows.cmd -NoRun
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$NoRun
)

$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot
$preset = if ($Configuration -eq 'Release') { 'windows-release' } else { 'windows-debug' }

if (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'CMakePresets.json'))) {
    throw "El lanzador debe estar en la raiz del repositorio, junto a CMakePresets.json ($repoRoot)."
}

# Priorizar la instalacion real si se conoce; detectar otras mediante vswhere.
$vsCandidates = @()
if ($env:VSINSTALLDIR) { $vsCandidates += $env:VSINSTALLDIR }
$vsCandidates += 'D:\Visual Studio'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $found = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $found) { $vsCandidates += @($found)[0].Trim() }
}
$vsRoot = $null
foreach ($candidate in $vsCandidates) {
    if (-not $candidate) { continue }
    $shell = Join-Path $candidate 'Common7\Tools\Launch-VsDevShell.ps1'
    if (Test-Path -LiteralPath $shell) { $vsRoot = $candidate; break }
}
if (-not $vsRoot) {
    throw 'No se encuentra Visual Studio con herramientas de C++. Instala Desktop development with C++ y Windows SDK.'
}
$devShell = Join-Path $vsRoot 'Common7\Tools\Launch-VsDevShell.ps1'

# Localizar CMake y Ninja: instalacion de VS o comandos ya disponibles en PATH.
$cmakeCandidate = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninjaCandidate = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if (Test-Path -LiteralPath $cmakeCandidate) { $cmake = $cmakeCandidate }
else {
    $cmd = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if (-not $cmd) { throw 'No se encuentra CMake; instala las herramientas CMake para Visual Studio.' }
    $cmake = $cmd.Source
}
if ($env:NINJA -and (Test-Path -LiteralPath $env:NINJA)) { $ninja = $env:NINJA }
elseif (Test-Path -LiteralPath $ninjaCandidate) { $ninja = $ninjaCandidate }
else {
    $cmd = Get-Command ninja.exe -ErrorAction SilentlyContinue
    if (-not $cmd) { throw 'No se encuentra Ninja; instala las herramientas CMake/Ninja para Visual Studio.' }
    $ninja = $cmd.Source
}

# Respetar VCPKG_ROOT si el usuario ya lo definio; no fijar rutas de otro equipo.
$vcpkgCandidates = @($env:VCPKG_ROOT, (Join-Path $vsRoot 'VC\vcpkg'), (Join-Path $HOME 'vcpkg'))
$vcpkg = $null
foreach ($candidate in $vcpkgCandidates) {
    if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'scripts\buildsystems\vcpkg.cmake'))) {
        $vcpkg = $candidate; break
    }
}
if (-not $vcpkg) { throw 'No se encuentra vcpkg. Establece VCPKG_ROOT o instala vcpkg.' }

Write-Host '[EscapeTheUAB] Preparando compilador MSVC x64...' -ForegroundColor Cyan
& $devShell -Arch amd64 -HostArch amd64
if (-not $env:INCLUDE) { throw 'No se ha inicializado INCLUDE. Revisa el toolset MSVC y el Windows SDK.' }
$env:VCPKG_ROOT = $vcpkg
$env:NINJA = $ninja

Push-Location $repoRoot
try {
    Write-Host "[EscapeTheUAB] Configurando $preset..." -ForegroundColor Cyan
    & $cmake --preset $preset
    if ($LASTEXITCODE -ne 0) { throw "Fallo de configuracion CMake ($LASTEXITCODE)." }

    Write-Host "[EscapeTheUAB] Compilando $preset..." -ForegroundColor Cyan
    & $cmake --build --preset $preset --parallel
    if ($LASTEXITCODE -ne 0) { throw "Fallo de compilacion ($LASTEXITCODE). NO se ejecutara el .exe antiguo." }

    $exe = Join-Path $repoRoot "build\$preset\bin\EntornVGI.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "No se ha encontrado el ejecutable: $exe" }
    if ($NoRun) {
        Write-Host '[EscapeTheUAB] Compilacion completada (sin ejecutar).' -ForegroundColor Green
    } else {
        Write-Host '[EscapeTheUAB] Iniciando juego recien compilado...' -ForegroundColor Green
        & $exe
        if ($LASTEXITCODE -ne 0) { throw "El juego termino con codigo $LASTEXITCODE." }
    }
}
finally { Pop-Location }
