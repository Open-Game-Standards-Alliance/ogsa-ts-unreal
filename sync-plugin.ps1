# sync-plugin.ps1 — mirror this plugin into a UE project's Plugins/ folder.
#
# UE test projects consume a *copy* of the plugin, which silently drifts
# behind this repo. Run this after every plugin change:
#
#   ./sync-plugin.ps1 -Project "C:\path\to\MyProject"           # sync only
#   ./sync-plugin.ps1 -Project "C:\path\to\MyProject" -Build    # + build Editor target
#
# -Project accepts the project directory or the .uproject file itself.
# -Build discovers the newest installed engine for the current platform
# unless -EngineDir overrides it.
#
# Only OgsaTelemetry.uplugin and Source/ are mirrored (exact mirror —
# stale files in the copy are deleted); repo docs/tests/tools stay out.
#
# Cross-platform: runs under Windows PowerShell 5.1 or pwsh (Linux/macOS,
# e.g. a self-hosted runner with UE installed). Uses a pure-PowerShell
# mirror instead of robocopy, which is Windows-only, and picks the right
# Build.bat/Build.sh and target platform for the host OS.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Project,
    [switch]$Build,
    [string]$EngineDir
)

$ErrorActionPreference = 'Stop'

# $IsWindows/$IsMacOS/$IsLinux only exist under pwsh (PowerShell Core);
# Windows PowerShell 5.1 (Desktop edition) has no such variables and only
# ever runs on Windows, so treat "undefined" as Windows.
$onWindows = (-not (Test-Path variable:IsWindows)) -or $IsWindows
$onMacOS = (Test-Path variable:IsMacOS) -and $IsMacOS
$onLinux = (Test-Path variable:IsLinux) -and $IsLinux

function Sync-Mirror {
    # Mirrors $Source into $Destination: copies new/changed files, deletes
    # stale files in the destination that no longer exist in the source,
    # and prunes directories left empty by deletions.
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $srcFull = (Resolve-Path $Source).Path
    $dstFull = (Resolve-Path $Destination).Path

    $relPaths = @()
    foreach ($f in Get-ChildItem -Path $srcFull -Recurse -File) {
        $rel = $f.FullName.Substring($srcFull.Length).TrimStart('\', '/')
        $relPaths += $rel
        $dstFile = Join-Path $dstFull $rel
        $dstDir = Split-Path $dstFile -Parent
        if (-not (Test-Path $dstDir)) { New-Item -ItemType Directory -Force -Path $dstDir | Out-Null }
        $copy = $true
        if (Test-Path $dstFile) {
            $d = Get-Item $dstFile
            if ($d.Length -eq $f.Length -and $d.LastWriteTimeUtc -ge $f.LastWriteTimeUtc) { $copy = $false }
        }
        if ($copy) { Copy-Item -Path $f.FullName -Destination $dstFile -Force }
    }

    foreach ($f in Get-ChildItem -Path $dstFull -Recurse -File) {
        $rel = $f.FullName.Substring($dstFull.Length).TrimStart('\', '/')
        if ($relPaths -notcontains $rel) { Remove-Item -Path $f.FullName -Force }
    }

    Get-ChildItem -Path $dstFull -Recurse -Directory |
        Sort-Object -Property FullName -Descending |
        Where-Object { (Get-ChildItem -Path $_.FullName -Force | Measure-Object).Count -eq 0 } |
        Remove-Item -Force
}

$pluginDir = $PSScriptRoot
$uplugin = Join-Path $pluginDir 'OgsaTelemetry.uplugin'
if (-not (Test-Path $uplugin)) { throw "not run from the plugin root: $uplugin missing" }

if ($Project.EndsWith('.uproject')) { $Project = Split-Path $Project -Parent }
$Project = (Resolve-Path $Project).Path
$uprojectFile = Get-ChildItem $Project -Filter '*.uproject' | Select-Object -First 1
if (-not $uprojectFile) { throw "no .uproject found in $Project" }

$dst = Join-Path $Project 'Plugins/OgsaTelemetry'
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Copy-Item -Path $uplugin -Destination (Join-Path $dst 'OgsaTelemetry.uplugin') -Force
Sync-Mirror -Source (Join-Path $pluginDir 'Source') -Destination (Join-Path $dst 'Source')
Write-Host "synced -> $dst"

if ($Build) {
    if ($onWindows) {
        $platform = 'Win64'
        $buildScriptRel = 'Engine/Build/BatchFiles/Build.bat'
    } elseif ($onMacOS) {
        $platform = 'Mac'
        $buildScriptRel = 'Engine/Build/BatchFiles/Mac/Build.sh'
    } else {
        $platform = 'Linux'
        $buildScriptRel = 'Engine/Build/BatchFiles/Linux/Build.sh'
    }

    if (-not $EngineDir) {
        $cands = @()
        if ($onWindows) {
            # registry first (validate: stale entries point at missing dirs),
            # then the standard launcher install glob
            $cands += @(Get-ItemProperty 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\*' -ErrorAction SilentlyContinue |
                Where-Object { $_.InstalledDirectory -and (Test-Path (Join-Path $_.InstalledDirectory $buildScriptRel)) } |
                Select-Object -ExpandProperty InstalledDirectory)
            $cands += @(Get-ChildItem 'C:\Program Files\Epic Games\UE_*' -Directory -ErrorAction SilentlyContinue |
                Where-Object { Test-Path (Join-Path $_.FullName $buildScriptRel) } |
                Select-Object -ExpandProperty FullName)
        } elseif ($onMacOS) {
            # standard launcher install locations
            $cands += @(Get-ChildItem '/Users/Shared/Epic Games/UE_*' -Directory -ErrorAction SilentlyContinue |
                Where-Object { Test-Path (Join-Path $_.FullName $buildScriptRel) } |
                Select-Object -ExpandProperty FullName)
        } else {
            # Linux has no launcher installer; common source-build / extracted locations
            $cands += @(Get-ChildItem "$HOME/UnrealEngine*", '/opt/UnrealEngine*', '/opt/unreal-engine*' -Directory -ErrorAction SilentlyContinue |
                Where-Object { Test-Path (Join-Path $_.FullName $buildScriptRel) } |
                Select-Object -ExpandProperty FullName)
        }
        $EngineDir = $cands | Sort-Object -Unique | Select-Object -Last 1
    }
    if (-not $EngineDir -or -not (Test-Path $EngineDir)) { throw "no engine found; pass -EngineDir" }

    $buildScript = Join-Path $EngineDir $buildScriptRel
    $name = $uprojectFile.BaseName
    if ($onWindows) {
        & $buildScript "${name}Editor" $platform Development `
            -project="$($uprojectFile.FullName)" -WaitMutex
    } else {
        # Build.sh on Mac/Linux has no -WaitMutex flag; ensure it's
        # executable (idempotent — the installed copy may not have +x set).
        & chmod +x $buildScript
        & $buildScript "${name}Editor" $platform Development `
            -project="$($uprojectFile.FullName)"
    }
    if ($LASTEXITCODE -ne 0) { throw "editor target build failed ($LASTEXITCODE)" }
}
exit 0
