#requires -Version 5.1
<#
.SYNOPSIS
Installs cpsc and its HTML manual for the current user.
.DESCRIPTION
Copies cpsc.exe and doc\cpsc.html, creates a Start Menu help shortcut, and
adds the installation directory to the user PATH. No administrator is needed
for the default location: %LOCALAPPDATA%\Programs\cpsc.
.EXAMPLE
.\install.ps1 -SourceDir .\build
.EXAMPLE
.\install.ps1 -Prefix 'D:\My Programs\cpsc'
#>
[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$Prefix,
    [ValidateNotNullOrEmpty()]
    [string]$SourceDir = $PSScriptRoot,
    [switch]$Help
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

if ($Help) {
    Write-Output 'Usage: install.ps1 [-Prefix DIR] [-SourceDir DIR] [-Help]'
    Write-Output 'Default destination: %LOCALAPPDATA%\Programs\cpsc'
    Write-Output 'Source must contain cpsc.exe and doc\cpsc.html; default: script directory.'
    return
}

function Test-PathEntry([string]$PathValue, [string]$Directory) {
    $key = $Directory.Replace('/', '\').TrimEnd('\')
    foreach ($entry in ($PathValue -split ';')) {
        $expanded = [Environment]::ExpandEnvironmentVariables($entry.Trim().Trim('"'))
        if ($expanded.Replace('/', '\').TrimEnd('\') -ieq $key) {
            return $true
        }
    }
    return $false
}

try {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw 'This installer requires Windows.'
    }
    if (-not [string]::IsNullOrEmpty($env:DESTDIR)) {
        throw 'DESTDIR staging is unsupported on Windows; use -Prefix instead.'
    }
    if (-not $PSBoundParameters.ContainsKey('Prefix')) {
        $localAppData = [Environment]::GetFolderPath('LocalApplicationData')
        if ([string]::IsNullOrWhiteSpace($localAppData)) {
            throw 'Cannot locate the current user application directory; specify -Prefix.'
        }
        $Prefix = Join-Path $localAppData 'Programs\cpsc'
    }
    if ($Prefix -notmatch '^(?:[A-Za-z]:[\\/]|\\\\[^\\]+\\[^\\]+)' -or $Prefix.Contains(';')) {
        throw '-Prefix must be an absolute Windows path without a semicolon.'
    }
    $Prefix = [IO.Path]::GetFullPath($Prefix)
    if (-not (Test-Path -LiteralPath $SourceDir -PathType Container)) {
        throw "Cannot open source directory: $SourceDir"
    }
    $SourceDir = (Resolve-Path -LiteralPath $SourceDir).ProviderPath
    $sourceExe = Join-Path $SourceDir 'cpsc.exe'
    $sourceManual = Join-Path $SourceDir 'doc\cpsc.html'
    foreach ($source in @($sourceExe, $sourceManual)) {
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Missing source file: $source; build cpsc and the doc target first."
        }
    }

    $docDir = Join-Path $Prefix 'doc'
    $targetExe = Join-Path $Prefix 'cpsc.exe'
    $targetManual = Join-Path $docDir 'cpsc.html'
    foreach ($directory in @($Prefix, $docDir)) {
        if ((Test-Path -LiteralPath $directory) -and
            -not (Test-Path -LiteralPath $directory -PathType Container)) {
            throw "Installation directory is occupied by a file: $directory"
        }
    }
    foreach ($file in @($targetExe, $targetManual)) {
        if (Test-Path -LiteralPath $file -PathType Container) {
            throw "Installation file is occupied by a directory: $file"
        }
    }
    $programs = [Environment]::GetFolderPath('Programs')
    if ([string]::IsNullOrWhiteSpace($programs)) {
        throw 'Cannot locate the current user Start Menu.'
    }
    $shortcutDir = Join-Path $programs 'cpsc'
    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $shell = New-Object -ComObject WScript.Shell

    # Directory.CreateDirectory treats paths literally, including spaces and brackets.
    [IO.Directory]::CreateDirectory($docDir) | Out-Null
    if ($sourceExe -ine $targetExe) {
        Copy-Item -LiteralPath $sourceExe -Destination $targetExe -Force
    }
    if ($sourceManual -ine $targetManual) {
        Copy-Item -LiteralPath $sourceManual -Destination $targetManual -Force
    }
    [IO.Directory]::CreateDirectory($shortcutDir) | Out-Null
    $shortcut = $shell.CreateShortcut((Join-Path $shortcutDir 'cpsc Help.lnk'))
    $shortcut.TargetPath = $targetManual
    $shortcut.WorkingDirectory = $docDir
    $shortcut.Description = 'cpsc command-line manual'
    $shortcut.Save()

    # Read only the user PATH; never persist the combined process/system PATH.
    if (-not (Test-PathEntry $userPath $Prefix)) {
        $separator = if ([string]::IsNullOrEmpty($userPath) -or $userPath.EndsWith(';')) { '' } else { ';' }
        [Environment]::SetEnvironmentVariable('Path', "$userPath$separator$Prefix", 'User')
    }
    if (-not (Test-PathEntry $env:PATH $Prefix)) {
        $env:PATH = "$env:PATH;$Prefix"
    }
    Write-Output "Installed $targetExe"
    Write-Output "Installed $targetManual"
    Write-Output 'Open the manual from Start Menu > cpsc > cpsc Help.'
    Write-Output 'Open a new terminal to use cpsc by name (existing parent terminals keep their old PATH).'
}
catch {
    Write-Error "install.ps1: $($_.Exception.Message) Check the destination permissions and supplied paths." -ErrorAction Continue
    exit 1
}
