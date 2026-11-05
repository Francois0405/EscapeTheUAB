param(
    [Parameter(Mandatory = $true)][string]$LauncherPath,
    [Parameter(Mandatory = $true)][string]$StubPath
)

$ErrorActionPreference = 'Stop'
$testRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '.launcher-test-temp'))
$repoRoot = Join-Path $testRoot 'repo'
$vsRoot = Join-Path $testRoot 'visual-studio'
$sentinel = Join-Path $testRoot 'stale-exe-was-launched.txt'
$oldVsInstall = $env:VSINSTALLDIR
$oldVcpkgRoot = $env:VCPKG_ROOT
$oldSentinel = $env:ESCAPETHEUAB_LAUNCHER_TEST_SENTINEL

try {
    if (Test-Path -LiteralPath $testRoot) {
        throw "El directorio temporal ya existe; no se sobrescribira: $testRoot"
    }
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $vsRoot 'Common7\Tools'), `
        (Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'), `
        (Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'), `
        (Join-Path $vsRoot 'VC\vcpkg\scripts\buildsystems'), `
        (Join-Path $repoRoot 'build\windows-debug\bin') | Out-Null

    Copy-Item -LiteralPath $LauncherPath -Destination (Join-Path $repoRoot 'run-windows.ps1')
    Set-Content -LiteralPath (Join-Path $repoRoot 'CMakePresets.json') -Value '{"version":6}' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $vsRoot 'Common7\Tools\Launch-VsDevShell.ps1') `
        -Value "param([string]`$Arch,[string]`$HostArch); `$env:INCLUDE='mock-sdk'" -Encoding ascii
    Set-Content -LiteralPath (Join-Path $vsRoot 'VC\vcpkg\scripts\buildsystems\vcpkg.cmake') `
        -Value '# test stub' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe') `
        -Value 'not executed by this test' -Encoding ascii
    Copy-Item -LiteralPath $StubPath -Destination `
        (Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
    Copy-Item -LiteralPath $StubPath -Destination `
        (Join-Path $repoRoot 'build\windows-debug\bin\EntornVGI.exe')

    $env:VSINSTALLDIR = $vsRoot
    $env:VCPKG_ROOT = Join-Path $vsRoot 'VC\vcpkg'
    $env:ESCAPETHEUAB_LAUNCHER_TEST_SENTINEL = $sentinel
    $powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $childScript = Join-Path $repoRoot 'run-windows.ps1'
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $powershell
    $startInfo.Arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$childScript`" -NoRun"
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $childProcess = [Diagnostics.Process]::Start($startInfo)
    $stdout = $childProcess.StandardOutput.ReadToEnd()
    $stderr = $childProcess.StandardError.ReadToEnd()
    $childProcess.WaitForExit()
    $exitCode = $childProcess.ExitCode
    $output = $stdout + [Environment]::NewLine + $stderr
    $childProcess.Dispose()

    if ($exitCode -eq 0) {
        throw 'El lanzador debia devolver error cuando falla cmake --build.'
    }
    if (Test-Path -LiteralPath $sentinel) {
        throw 'El lanzador ejecuto el EXE antiguo tras fallar la compilacion.'
    }
    if (($output | Out-String) -notmatch 'Fallo de compilacion') {
        throw "El error no fue claro. Salida: $($output | Out-String)"
    }

    'WindowsLauncherDoesNotRunStaleBuild: OK'
}
finally {
    $env:VSINSTALLDIR = $oldVsInstall
    $env:VCPKG_ROOT = $oldVcpkgRoot
    $env:ESCAPETHEUAB_LAUNCHER_TEST_SENTINEL = $oldSentinel
    if (Test-Path -LiteralPath $testRoot) {
        $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
        $resolvedParent = [IO.Path]::GetFullPath($PSScriptRoot)
        if (-not $resolvedTestRoot.StartsWith($resolvedParent, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Ruta temporal fuera de tests/: $resolvedTestRoot"
        }
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}
