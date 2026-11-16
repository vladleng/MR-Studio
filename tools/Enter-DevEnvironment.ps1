[CmdletBinding()]
param(
    [string]$VsInstallationPath
)

$ErrorActionPreference = 'Stop'

if (!$VsInstallationPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) {
        throw 'Visual Studio Installer/vswhere.exe is missing. Install Visual Studio with C++ tools.'
    }
    $VsInstallationPath = & $vswhere -latest -products '*' -version '[18.0,19.0)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or !$VsInstallationPath) {
        throw 'A complete Visual Studio 18 installation with MSVC x64 tools was not found.'
    }
}

$launcher = Join-Path $VsInstallationPath 'Common7/Tools/Launch-VsDevShell.ps1'
if (!(Test-Path -LiteralPath $launcher)) {
    throw "Developer PowerShell launcher is missing: $launcher"
}
& $launcher -VsInstallationPath $VsInstallationPath -Arch amd64 -HostArch amd64 `
    -SkipAutomaticLocation -NoLogo

# Add the tools from this VS installation without changing the user's global PATH.
$cmakeTools = Join-Path $VsInstallationPath 'Common7/IDE/CommonExtensions/Microsoft/CMake'
foreach ($relativePath in @('CMake/bin', 'Ninja')) {
    $toolDirectory = Join-Path $cmakeTools $relativePath
    if (!(Test-Path -LiteralPath $toolDirectory)) {
        throw "Visual Studio CMake component is missing: $toolDirectory"
    }
    if (($env:PATH -split ';') -notcontains $toolDirectory) {
        $env:PATH = "$toolDirectory;$env:PATH"
    }
}

if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64' -or $env:VSCMD_ARG_HOST_ARCH -ne 'x64') {
    throw 'Developer PowerShell did not initialize the x64 host and target.'
}
if (!$env:WindowsSdkDir -or !$env:WindowsSDKVersion) {
    throw 'Windows SDK environment variables are missing.'
}
foreach ($relativePath in @(
    "Include/$($env:WindowsSDKVersion)/um/Windows.h",
    "Include/$($env:WindowsSDKVersion)/ucrt/stdio.h",
    "Lib/$($env:WindowsSDKVersion)/um/x64/kernel32.lib"
)) {
    $sdkFile = Join-Path $env:WindowsSdkDir $relativePath
    if (!(Test-Path -LiteralPath $sdkFile)) {
        throw "Windows SDK file is missing: $sdkFile"
    }
}

$commands = foreach ($name in @('cl.exe', 'link.exe', 'rc.exe', 'msbuild.exe', 'cmake.exe', 'ctest.exe', 'ninja.exe', 'git.exe', 'gh.exe', 'python.exe')) {
    $command = Get-Command $name -CommandType Application -ErrorAction Stop | Select-Object -First 1
    [pscustomobject]@{ Tool = $name; Path = $command.Source }
}
Write-Host "MR Studio developer environment: x64 / Windows SDK $($env:WindowsSDKVersion.TrimEnd('\'))"
$commands | Format-Table -AutoSize
