[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$ValidationDirectory,
    [switch]$ValidateOnly
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$validation = [IO.Path]::GetFullPath((Join-Path $taskRoot $ValidationDirectory))
$validationRoot = [IO.Path]::GetFullPath((Join-Path $taskRoot 'build\coo')) + '\'
if (-not $validation.StartsWith($validationRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Validation output must stay under build/coo.'
}
$receiptPath = Join-Path $validation 'installation.json'
if (Test-Path -LiteralPath $receiptPath) { throw 'This candidate has an installation receipt; preserve that evidence.' }
$manifest = Get-Content -Raw -LiteralPath (Join-Path $validation 'package.json') | ConvertFrom-Json
$names = @('steam_api64.dll', 'steam_api64.pdb', 'Lua_LICENSE.txt',
    'Dawn/scripts/one_au.lua', 'Dawn/scripts/homecoming.lua', 'Dawn/scripts/adieu.lua', 'Dawn/scripts/omega.lua', 'Dawn/scripts/deadly_trial.lua', 'Dawn/scripts/gateway.lua', 'Dawn/scripts/beyond_infinity.lua', 'Dawn/scripts/deep_storage.lua', 'Dawn/scripts/hijacked.lua', 'Dawn/scripts/strike_pact.lua', 'Dawn/scripts/strike_bond.lua', 'Dawn/scripts/mission_pact.lua', 'Dawn/scripts/mission_bond.lua', 'Dawn/scripts/eater_of_worlds.lua', 'Dawn/scripts/mercury_freeroam.json',
    'Dawn/scripts/eden_freeroam.json', 'Dawn/scripts/fleet_freeroam.json', 'Dawn/scripts/polaris_freeroam.json', 'Dawn/scripts/planet_x_freeroam.json', 'Dawn/scripts/tangled_shore_freeroam.json', 'Dawn/scripts/dreaming_city_freeroam.json', 'Dawn/scripts/infinite_abyss.json')
$scopeProperty = $manifest.PSObject.Properties['validationScope']
$validationScope = if ($scopeProperty) { [string]$scopeProperty.Value } else { 'full-lua' }
$expectedCount = switch ($validationScope) {
    'full-lua' { 119 }
    'full-lua-release' { 78 }
    'hijacked-release' { 3 }
    'deep-storage-release' { 2 }
    'mercury-reentry-release' { 2 }
    'eater-reactor-release' { 5 }
    'eater-contest-release' { 9 }
    default { throw 'Unknown validation scope.' }
}
if ($manifest.format -ne 1 -or @($manifest.files.PSObject.Properties).Count -ne $names.Count -or $manifest.buildsAndTests -ne $expectedCount) {
    throw 'Package does not match its declared validation scope.'
}
$results = @(Get-Content -Raw -LiteralPath (Join-Path $validation 'results.json') | ConvertFrom-Json)
$failures = @(Get-Content -Raw -LiteralPath (Join-Path $validation 'failures.json') | ConvertFrom-Json)
$expectedJobs = @($manifest.validationJobs)
if ($results.Count -ne $expectedCount -or $failures.Count -ne 0 -or $expectedJobs.Count -ne $expectedCount) {
    throw 'Mission validation is incomplete or failed.'
}
$actualJobs = @($results | ForEach-Object { [string]$_.project + '|' + [string]$_.configuration } | Sort-Object -Unique)
if ($actualJobs.Count -ne $expectedCount -or (Compare-Object -ReferenceObject @($expectedJobs | Sort-Object) -DifferenceObject $actualJobs)) {
    throw 'Validation result identities do not match the packaged scope.'
}
if ($validationScope -eq 'full-lua-release') {
    $nonReleaseResults = @($results | Where-Object { $_.configuration -ne 'Release' })
    $nonReleaseJobs = @($expectedJobs | Where-Object { $_ -notmatch '\|Release$' })
    if ($nonReleaseResults.Count -ne 0 -or $nonReleaseJobs.Count -ne 0) {
        throw 'Release-only validation contains a non-Release job.'
    }
}
if ($validationScope -in @('hijacked-release', 'deep-storage-release', 'mercury-reentry-release', 'eater-reactor-release', 'eater-contest-release')) {
    $missionProjects = switch ($validationScope) {
        'hijacked-release' { @('hijacked_tests', 'hijacked_catalog_tests', 'Dawn') }
        'deep-storage-release' { @('deep_storage_tests', 'Dawn') }
        'mercury-reentry-release' { @('retained_authority_scope_tests', 'Dawn') }
        'eater-reactor-release' { @('eater_of_worlds_tests', 'eater_of_worlds_roster_tests', 'player_position_tests', 'other_mission_protocol_tests', 'Dawn') }
        'eater-contest-release' { @('eater_of_worlds_tests', 'eater_of_worlds_roster_tests', 'player_position_tests', 'other_mission_protocol_tests', 'native_nightfall_power_tests', 'nightfall_rules_tests', 'mission_launch_lifecycle_tests', 'mission_launch_visual_tests', 'Dawn') }
    }
    foreach ($project in $missionProjects) {
        if (@($results | Where-Object { $_.project -eq $project -and $_.configuration -eq 'Release' }).Count -ne 1) {
            throw "Missing or duplicate mission Release result: $project"
        }
    }
}
if ((Get-FileHash -LiteralPath (Join-Path $validation 'source-manifest.json')).Hash -ne $manifest.sourceManifestSha256) {
    throw 'Source manifest changed after packaging.'
}
$before = @{}
foreach ($name in $names) {
    $expected = $manifest.files.PSObject.Properties[$name]
    $source = Join-Path $validation ('payload/' + $name)
    if (-not $expected -or (Get-FileHash -LiteralPath $source).Hash -ne $expected.Value) { throw "Payload changed: $name" }
    $target = Join-Path $taskRoot $name
    $previous = $manifest.previousFiles.PSObject.Properties[$name]
    if (Test-Path -LiteralPath $target) {
        $before[$name] = (Get-FileHash -LiteralPath $target).Hash
        if (-not $previous -or $before[$name] -ne $previous.Value) { throw "Installed file changed since packaging: $name" }
    } elseif ($previous) { throw "Installed file disappeared since packaging: $name" }
}
if (-not $before.ContainsKey('steam_api64.dll')) { throw 'Expected an installed DLL to back up.' }
if ($ValidateOnly) { Write-Output 'Package and installed baseline verified. No files changed.'; return }
if (Get-Process -Name destiny2 -ErrorAction SilentlyContinue) { throw 'Close Destiny 2 before installation.' }
$backup = Join-Path $taskRoot ('.dawn\backups\lua-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach ($name in $before.Keys) {
    $saved = Join-Path $backup $name
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $saved) | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskRoot $name) -Destination $saved
    if ((Get-FileHash -LiteralPath $saved).Hash -ne $before[$name]) { throw "Backup failed: $name" }
}
$before | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backup 'files.json') -Encoding utf8
if (Get-Process -Name destiny2 -ErrorAction SilentlyContinue) { throw 'Destiny 2 started during staging.' }
foreach ($name in $names) {
    $target = Join-Path $taskRoot $name
    if ($before.ContainsKey($name)) {
        if ((Get-FileHash -LiteralPath $target).Hash -ne $before[$name]) { throw "Installed file changed during staging: $name" }
    } elseif (Test-Path -LiteralPath $target) { throw "Installed file appeared during staging: $name" }
}
$copied = [Collections.Generic.List[string]]::new()
try {
    foreach ($name in $names) {
        if (Get-Process -Name destiny2 -ErrorAction SilentlyContinue) { throw 'Destiny 2 started during installation.' }
        $target = Join-Path $taskRoot $name
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        $copied.Add($name)
        Copy-Item -LiteralPath (Join-Path $validation ('payload/' + $name)) -Destination $target -Force
        if ((Get-FileHash -LiteralPath $target).Hash -ne $manifest.files.PSObject.Properties[$name].Value) { throw "Installed hash mismatch: $name" }
    }
    if (Get-Process -Name destiny2 -ErrorAction SilentlyContinue) { throw 'Destiny 2 started before installation completed.' }
[ordered]@{status='installed';utc=[DateTime]::UtcNow.ToString('o');files=$manifest.files;
    previousFiles=$before;backup=$backup;buildsAndTests=$manifest.buildsAndTests;compilerWarnings=0;validationScope=$validationScope;
    nativePlaythrough='not_yet_verified';gameLaunched=$false
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $receiptPath -Encoding utf8
} catch {
    $installFailure = $_.Exception.Message
    $restoreFailures = [Collections.Generic.List[string]]::new()
    for ($restoreIndex = $copied.Count - 1; $restoreIndex -ge 0; --$restoreIndex) {
        $name = $copied[$restoreIndex]
        try {
            $target = [IO.Path]::GetFullPath((Join-Path $taskRoot $name))
            if (-not $target.StartsWith($taskRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Rollback target outside workspace.' }
            if ($before.ContainsKey($name)) {
                Copy-Item -LiteralPath (Join-Path $backup $name) -Destination $target -Force
                if ((Get-FileHash -LiteralPath $target).Hash -ne $before[$name]) { throw 'Restored file hash mismatch.' }
            } elseif (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target }
        } catch { $restoreFailures.Add($name + ': ' + $_.Exception.Message) }
    }
    try { if (Test-Path -LiteralPath $receiptPath) { Remove-Item -LiteralPath $receiptPath } }
    catch { $restoreFailures.Add('Installation receipt: ' + $_.Exception.Message) }
    if ($restoreFailures.Count) {
        throw ("Installation failed: $installFailure. Close Destiny and restore from $backup. Rollback failures: " + ($restoreFailures -join '; '))
    }
    throw "Installation failed and previous files were restored. Backup: $backup. Cause: $installFailure"
}

Write-Output "Installed validated Lua missions. Backup: $backup"
Write-Output 'Launch through your usual command. Scripts reload on the next game process.'
