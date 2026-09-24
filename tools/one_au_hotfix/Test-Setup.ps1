#requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory)][string]$SetupPath)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('Dawn-hotfix-setup-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
Copy-Item -LiteralPath $SetupPath -Destination (Join-Path $root 'Setup-Dawn.ps1')
$stub=@'
[CmdletBinding(SupportsShouldProcess=$true)]
param([string]$GameRoot,[switch]$Update,[switch]$Restore,[string]$BackupPath)
$global:DawnHotfixSetupCall=@{}
foreach($key in $PSBoundParameters.Keys){$global:DawnHotfixSetupCall[$key]=$PSBoundParameters[$key]}
'@
[IO.File]::WriteAllText((Join-Path $root 'Install-Dawn.ps1'),$stub)
$cases=@('fresh','settings','database','incomplete','receipt','runtime-file','restore','bad-backup','whatif')
foreach($case in $cases) {
    $game=Join-Path $root $case
    [IO.Directory]::CreateDirectory($game) | Out-Null
    [IO.File]::WriteAllText((Join-Path $game 'destiny2.exe'),'fixture - never executed')
    $args=@{GameRoot=$game}
    if($case -in @('settings','database','incomplete')) {
        $runtime=if($case -eq 'database'){Join-Path $game 'bin/x64/Dawn'}else{Join-Path $game 'Dawn'}
        [IO.Directory]::CreateDirectory($runtime) | Out-Null
        $name=switch($case){settings{'settings.json'} database{'player-state.db'} incomplete{'orphan.txt'}}
        [IO.File]::WriteAllText((Join-Path $runtime $name),'sentinel')
    }
    if($case -eq 'receipt') {
        [IO.Directory]::CreateDirectory((Join-Path $game '.dawn')) | Out-Null
        [IO.File]::WriteAllText((Join-Path $game '.dawn/release.json'),'{}')
    }
    if($case -eq 'runtime-file'){[IO.File]::WriteAllText((Join-Path $game 'Dawn'),'sentinel')}
    if($case -eq 'restore'){$args.Restore=$true;$args.BackupPath='fixture-backup'}
    if($case -eq 'bad-backup'){$args.BackupPath='fixture-backup'}
    if($case -eq 'whatif'){$args.WhatIf=$true;$args.Confirm=$false}
    $global:DawnHotfixSetupCall=$null;$failed=$false
    try { & (Join-Path $root 'Setup-Dawn.ps1') @args } catch {$failed=$true}
    $reject=$case -in @('incomplete','receipt','runtime-file','bad-backup')
    if($failed -ne $reject){throw "Unexpected setup result: $case"}
    if($reject){if($null -ne $global:DawnHotfixSetupCall){throw "Rejected setup invoked installer: $case"};continue}
    $call=$global:DawnHotfixSetupCall
    if($null -eq $call -or $call.GameRoot -ne $game){throw "Wrong setup target: $case"}
    $update=$call.ContainsKey('Update') -and $call.Update
    if($update -ne ($case -in @('settings','database'))){throw "Wrong profile mode: $case"}
    if($case -eq 'restore' -and (-not $call.Restore -or $call.BackupPath -ne 'fixture-backup')){throw 'Restore arguments lost'}
    if($case -eq 'whatif' -and (-not $call.WhatIf -or $call.Confirm)){throw 'WhatIf/Confirm arguments lost'}
}
Remove-Variable -Name DawnHotfixSetupCall -Scope Global
Write-Output "PASS: $($cases.Count) installer selection/forwarding cases; disposable fixtures: $root"
