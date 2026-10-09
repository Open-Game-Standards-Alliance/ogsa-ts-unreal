# sync-plugin.ps1 — mirror this plugin into a UE project's Plugins/ folder.
#
# UE test projects consume a *copy* of the plugin, which silently drifts
# behind this repo. Run this after every plugin change:
#
#   ./sync-plugin.ps1 -Project "C:\path\to\MyProject"           # sync only
#   ./sync-plugin.ps1 -Project "C:\path\to\MyProject" -Build    # + build Editor target
#
# -Project accepts the project directory or the .uproject file itself.
# -Build discovers the newest installed engine from the registry unless
# -EngineDir overrides it.
#
# Only OgsaTelemetry.uplugin and Source/ are mirrored (exact mirror —
# stale files in the copy are deleted); repo docs/tests/tools stay out.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Project,
    [switch]$Build,
    [string]$EngineDir
)

$ErrorActionPreference = 'Stop'

$pluginDir = $PSScriptRoot
$uplugin = Join-Path $pluginDir 'OgsaTelemetry.uplugin'
if (-not (Test-Path $uplugin)) { throw "not run from the plugin root: $uplugin missing" }

if ($Project.EndsWith('.uproject')) { $Project = Split-Path $Project -Parent }
$Project = (Resolve-Path $Project).Path
$uprojectFile = Get-ChildItem $Project -Filter '*.uproject' | Select-Object -First 1
if (-not $uprojectFile) { throw "no .uproject found in $Project" }

$dst = Join-Path $Project 'Plugins\OgsaTelemetry'
robocopy $pluginDir $dst 'OgsaTelemetry.uplugin' /NJH /NDL /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
robocopy (Join-Path $pluginDir 'Source') (Join-Path $dst 'Source') /MIR /NJH /NDL /NP
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
$LASTEXITCODE = 0
Write-Host "synced -> $dst"

if ($Build) {
    if (-not $EngineDir) {
        $EngineDir = (Get-ItemProperty 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\*' -ErrorAction SilentlyContinue |
            Sort-Object PSChildName -Descending | Select-Object -First 1).InstalledDirectory
    }
    if (-not $EngineDir -or -not (Test-Path $EngineDir)) { throw "no engine found; pass -EngineDir" }
    $name = $uprojectFile.BaseName
    & (Join-Path $EngineDir 'Engine\Build\BatchFiles\Build.bat') `
        "${name}Editor" Win64 Development `
        -project="$($uprojectFile.FullName)" -WaitMutex
    if ($LASTEXITCODE -ne 0) { throw "editor target build failed ($LASTEXITCODE)" }
}
