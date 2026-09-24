#requires -Version 5.1
<# Disposable fixtures only. Does not use or launch an installed game. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$testRoot = Join-Path $repo ('build/uninstaller-tests/' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$uninstaller = Join-Path $repo 'tools/install/release/Uninstall-Dawn.ps1'
$script:passed = 0
function Assert([bool] $Value, [string] $Message) { if (-not $Value) { throw "ASSERTION FAILED: $Message" } }
function Pass([string] $Message) { $script:passed++; Write-Host "PASS: $Message" }
function Write-File([string] $Path, [string] $Text) {
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    [IO.File]::WriteAllText($Path, $Text)
}
function Snapshot([string] $Root) {
    return (@(Get-ChildItem -LiteralPath $Root -File -Recurse -Force | Sort-Object FullName | ForEach-Object {
        $_.FullName.Substring($Root.Length) + ':' + (Get-FileHash -LiteralPath $_.FullName).Hash
    }) -join "`n")
}
function Expect-Failure([scriptblock] $Action, [string] $Pattern) {
    $message = $null
    try { & $Action } catch { $message = $_.Exception.Message }
    Assert ($null -ne $message -and $message -like $Pattern) "Expected $Pattern; received $message"
}

# Small DLLs with the same version-resource identity used for detection, no game code.
$dawnDll = Join-Path $testRoot 'dawn.dll'
$steamDll = Join-Path $testRoot 'steam.dll'
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
foreach ($fixture in @(@($dawnDll, 'Dawn'), @($steamDll, 'Steam Client API'))) {
    $source = $fixture[0] + '.cs'
    Write-File $source ('[assembly: System.Reflection.AssemblyProduct("' + $fixture[1] + '")] public class UninstallFixture {}')
    & $compiler /nologo /target:library /platform:x64 ("/out:" + $fixture[0]) $source
    if ($LASTEXITCODE -ne 0) { throw 'Could not compile the test fixture DLL.' }
}
function New-Game([string] $Name) {
    $game = Join-Path $testRoot $Name
    foreach ($relative in @('destiny2.exe', 'packages/keep.pkg', 'Sunrise/keep.txt', 'Restoration/keep.txt',
            'Dawn/player-state.db', 'Dawn/player-state.db-wal', 'Dawn/settings.json', 'Dawn/cache/cache.bin',
            'bin/x64/Dawn/player-state.db', 'bin/x64/Dawn/scripts/custom.lua',
            '.dawn/backup/old/Dawn/player-state.db', '.dawn/install-state.json', '.dawn/release.json')) {
        Write-File (Join-Path $game $relative) "fixture: $relative"
    }
    foreach ($relative in @('steam_api64.dll', 'bin/x64/steam_api64.dll')) {
        Copy-Item -LiteralPath $dawnDll -Destination (Join-Path $game $relative)
        Write-File ([IO.Path]::ChangeExtension((Join-Path $game $relative), '.pdb')) 'Dawn symbols'
    }
    return $game
}
function Assert-Uninstalled([string] $Game) {
    foreach ($relative in @('Dawn', 'bin/x64/Dawn', '.dawn', 'steam_api64.pdb', 'bin/x64/steam_api64.pdb')) {
        Assert (-not (Test-Path -LiteralPath (Join-Path $Game $relative))) "Uninstall retained $relative"
    }
    foreach ($relative in @('destiny2.exe', 'packages/keep.pkg', 'Sunrise/keep.txt', 'Restoration/keep.txt')) {
        Assert ([IO.File]::ReadAllText((Join-Path $Game $relative)) -ceq "fixture: $relative") "Uninstall changed $relative"
    }
}

$game = New-Game 'no-original [literal]'
$before = Snapshot $game
& $uninstaller -GameRoot $game -WhatIf
Assert ((Snapshot $game) -ceq $before) 'WhatIf changed files'
Pass 'Preview makes no changes, including paths containing brackets'
& $uninstaller -GameRoot $game
Assert-Uninstalled $game
Assert (-not (Test-Path -LiteralPath (Join-Path $game 'steam_api64.dll'))) 'Dawn DLL remains without an original'
Assert (-not (Test-Path -LiteralPath (Join-Path $game 'bin/x64/steam_api64.dll'))) 'Bin Dawn DLL remains'
$before = Snapshot $game
& $uninstaller -GameRoot $game
Assert ((Snapshot $game) -ceq $before) 'Repeat uninstall changed the game'
Pass 'Full uninstall deletes saves, caches, symbols, and all backups; repeat is harmless'

$game = New-Game 'original-backup'
[IO.Directory]::CreateDirectory((Join-Path $game '.dawn/original')) | Out-Null
Copy-Item -LiteralPath $steamDll -Destination (Join-Path $game '.dawn/original/steam_api64.dll')
& $uninstaller -GameRoot $game
Assert-Uninstalled $game
foreach ($relative in @('steam_api64.dll', 'bin/x64/steam_api64.dll')) {
    Assert ((Get-FileHash -LiteralPath (Join-Path $game $relative)).Hash -eq (Get-FileHash -LiteralPath $steamDll).Hash) 'Original DLL was not restored'
}
Pass 'Original Steam DLL is restored to both locations before every backup is deleted'

$game = New-Game 'release-history'
foreach ($stamp in @('001-original', '002-dawn-update')) {
    $history = Join-Path $game ".dawn/release-backups/$stamp"
    Write-File (Join-Path $history 'journal.json') (ConvertTo-Json @{ gameRoot = $game; state = 'complete' })
    foreach ($relative in @('steam_api64.dll', 'bin/x64/steam_api64.dll')) {
        $destination = Join-Path $history "previous/$relative"
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        $source = if ($stamp -eq '001-original') { $steamDll } else { $dawnDll }
        Copy-Item -LiteralPath $source -Destination $destination
    }
}
& $uninstaller -GameRoot $game
Assert-Uninstalled $game
Assert ((Get-FileHash -LiteralPath (Join-Path $game 'steam_api64.dll')).Hash -eq (Get-FileHash -LiteralPath $steamDll).Hash) 'An old Dawn DLL was restored as an original'
Pass 'Release backup lookup skips Dawn updates and restores the original Steam DLL'

$game = New-Game 'provided-original'
Copy-Item -LiteralPath $steamDll -Destination (Join-Path $game 'Dawn/original.dll')
& $uninstaller -GameRoot $game -OriginalDll (Join-Path $game 'Dawn/original.dll')
Assert-Uninstalled $game
Assert ((Get-FileHash -LiteralPath (Join-Path $game 'steam_api64.dll')).Hash -eq (Get-FileHash -LiteralPath $steamDll).Hash) 'Supplied original was not staged'
Pass 'Supplied original inside a removed runtime folder is staged before removal'

$game = New-Game 'other-dll'
Write-File (Join-Path $game 'steam_api64.dll') 'another mod'
& $uninstaller -GameRoot $game
Assert ([IO.File]::ReadAllText((Join-Path $game 'steam_api64.dll')) -ceq 'another mod') 'Unrecognized DLL was deleted'
Assert (-not (Test-Path -LiteralPath (Join-Path $game '.dawn'))) 'Backups were retained'
Pass 'Unrecognized DLLs are kept while Dawn data is purged'

$game = New-Game 'source-checkout'
Write-File (Join-Path $game 'Dawn/src/keep.cpp') 'source'
$before = Snapshot $game
Expect-Failure { & $uninstaller -GameRoot $game } '*source checkout*'
Assert ((Snapshot $game) -ceq $before) 'Source rejection changed files'
Pass 'Source checkout is rejected before mutation'

$game = New-Game 'interrupted-install'
Write-File (Join-Path $game '.dawn/release-backups/old/journal.json') (ConvertTo-Json @{ gameRoot = $game; state = 'installing' })
$before = Snapshot $game
Expect-Failure { & $uninstaller -GameRoot $game -OriginalDll $steamDll } '*Recover the interrupted*'
Assert ((Snapshot $game) -ceq $before) 'Incomplete installation backup was deleted'
Pass 'Interrupted installations are rejected even with an explicit original DLL'

$game = New-Game 'locked-file'
$before = Snapshot $game
$handle = [IO.File]::Open((Join-Path $game 'bin/x64/steam_api64.dll'), 'Open', 'Read', 'Read')
try { Expect-Failure { & $uninstaller -GameRoot $game -OriginalDll $steamDll } '*previous files were restored*' }
finally { $handle.Dispose() }
foreach ($relative in @('steam_api64.dll', 'bin/x64/steam_api64.dll')) {
    Assert ((Get-FileHash -LiteralPath (Join-Path $game $relative)).Hash -eq (Get-FileHash -LiteralPath $dawnDll).Hash) 'Failure did not restore the active Dawn DLL'
}
Assert (Test-Path -LiteralPath (Join-Path $game 'Dawn/player-state.db')) 'Failed uninstall lost the save'
& $uninstaller -GameRoot $game -OriginalDll $steamDll
Assert-Uninstalled $game
Pass 'A locked DLL rolls back; retry deletes temporary rollback files and all Dawn data'

$game = New-Game 'linked-backup'
$outside = Join-Path $testRoot 'outside'
Write-File (Join-Path $outside 'keep.txt') 'do not delete'
New-Item -ItemType Junction -Path (Join-Path $game '.dawn/linked') -Target $outside | Out-Null
Expect-Failure { & $uninstaller -GameRoot $game } '*Linked entry*'
Assert ([IO.File]::ReadAllText((Join-Path $outside 'keep.txt')) -ceq 'do not delete') 'Linked content was changed'
Assert (Test-Path -LiteralPath (Join-Path $game 'Dawn/player-state.db')) 'Link rejection changed the active profile'
Pass 'Linked backups are rejected without following them outside the game folder'

Write-Host "$script:passed checks passed on PowerShell $($PSVersionTable.PSVersion). Fixtures: $testRoot"
