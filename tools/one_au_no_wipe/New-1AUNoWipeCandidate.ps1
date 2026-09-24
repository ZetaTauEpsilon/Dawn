#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CandidateRoot,
    [Parameter(Mandatory)][string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath($CandidateRoot).TrimEnd('\', '/')
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/')
$zipPath = $outputRoot + '.zip'
if ($outputRoot -eq $sourceRoot -or $outputRoot.StartsWith($sourceRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must be separate from the input candidate.'
}
if ((Test-Path -LiteralPath $outputRoot) -or (Test-Path -LiteralPath $zipPath)) {
    throw 'Output already exists. Choose a new output directory.'
}
function Child-Path([string]$Root, [string]$Relative) {
    $path = [IO.Path]::GetFullPath((Join-Path $Root $Relative))
    if (-not $path.StartsWith($Root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Package path escapes its directory: $Relative" }
    return $path
}
function Sha256([byte[]]$Bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Hex-Bytes([string]$Hex) {
    if ($Hex.Length % 2 -ne 0 -or $Hex -notmatch '^[0-9a-f]+$') { throw 'Invalid patch hex.' }
    $bytes = New-Object byte[] ($Hex.Length / 2)
    for ($i = 0; $i -lt $bytes.Length; $i++) { $bytes[$i] = [Convert]::ToByte($Hex.Substring(2 * $i, 2), 16) }
    return ,$bytes
}
function Write-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), (New-Object Text.UTF8Encoding($false)))
}
$patch = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'candidate-patch.json') -Raw | ConvertFrom-Json
$release = Get-Content -LiteralPath (Child-Path $sourceRoot 'release.json') -Raw | ConvertFrom-Json
if ($patch.schema -ne 1 -or $release.schema -ne 1 -or $release.gameBuild -ne 86657 -or $release.runtimeDirectory -ne 'Dawn' -or $release.release -ne $patch.candidate) {
    throw 'Requires the original 0.1.3-1au-fix package with the entrance repairs already applied.'
}
$payload = Child-Path $sourceRoot 'payload'
$dll = [IO.File]::ReadAllBytes((Child-Path $payload 'steam_api64.dll'))
if ($dll.Length -ne $patch.size -or (Sha256 $dll) -ne $patch.originalSha256) { throw 'Unsupported DLL. Nothing was written.' }
$dllEntry = @($release.files | Where-Object { $_.path -eq 'steam_api64.dll' })
if ($dllEntry.Count -ne 1) { throw 'Expected exactly one DLL entry.' }
$known = @{}
foreach ($file in $release.files) {
    if ($known.ContainsKey([string]$file.path)) { throw 'Duplicate payload path.' }
    $known[[string]$file.path] = $true
    $path = Child-Path $payload $file.path
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -ne $file.size -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) {
        throw "Input payload failed validation: $($file.path)"
    }
}
$launchers = @('Install-Dawn.cmd', 'Install-Dawn.ps1', 'Update-Dawn.cmd', 'Update-Dawn.ps1', 'READ-ME.txt')
foreach ($name in $launchers) {
    if (-not (Test-Path -LiteralPath (Child-Path $sourceRoot $name) -PathType Leaf)) { throw "Missing candidate installer: $name" }
}
$provenance = @('candidate-patch.json', 'README.md', 'TESTING.md', 'verification.json', 'source-port.patch', 'test_candidate.py', 'New-1AUNoWipeCandidate.ps1', 'Test-1AUNoWipeCandidate.ps1')
foreach ($name in $provenance) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $name) -PathType Leaf)) { throw "Missing repair source: $name" }
}
$previousEnd = 0L
foreach ($range in $patch.ranges) {
    $before = Hex-Bytes $range.before; $after = Hex-Bytes $range.after; $offset = [long]$range.offset
    if ($offset -lt $previousEnd -or $before.Length -ne $after.Length -or $offset -gt $dll.Length - $before.Length) { throw 'Invalid or overlapping patch range.' }
    for ($i = 0; $i -lt $before.Length; $i++) { if ($dll[$offset + $i] -ne $before[$i]) { throw "Unexpected bytes at $offset" } }
    [Array]::Copy($after, 0, $dll, $offset, $after.Length)
    $previousEnd = $offset + $after.Length
}
if ((Sha256 $dll) -ne $patch.patchedSha256) { throw 'Final DLL checksum mismatch. Nothing was written.' }

# Validate the complete candidate and final DLL before creating any output.
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$outputPayload = Child-Path $outputRoot 'payload'
foreach ($file in $release.files) {
    $target = Child-Path $outputPayload $file.path
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    if ($file.path -eq 'steam_api64.dll') { [IO.File]::WriteAllBytes($target, $dll) }
    else { [IO.File]::Copy((Child-Path $payload $file.path), $target, $false) }
}
foreach ($name in $launchers) { [IO.File]::Copy((Child-Path $sourceRoot $name), (Child-Path $outputRoot $name), $false) }
$release.release = $patch.release; $release.createdUtc = [DateTime]::UtcNow.ToString('o')
$dllEntry[0].sha256 = $patch.patchedSha256
Write-Json (Join-Path $outputRoot 'release.json') $release
[IO.File]::Copy((Join-Path $PSScriptRoot 'TESTING.md'), (Join-Path $outputRoot 'RELEASE-NOTES.md'), $false)
$priorSource = Child-Path $sourceRoot 'repair-source'
if (Test-Path -LiteralPath $priorSource -PathType Container) { Copy-Item -LiteralPath $priorSource -Destination (Join-Path $outputRoot 'repair-source') -Recurse }
$repairOutput = Child-Path $outputRoot 'repair-source/no-wipe'
if (Test-Path -LiteralPath $repairOutput) { throw 'Input already contains a no-wipe repair.' }
[IO.Directory]::CreateDirectory($repairOutput) | Out-Null
foreach ($name in $provenance) { [IO.File]::Copy((Join-Path $PSScriptRoot $name), (Join-Path $repairOutput $name), $false) }
Write-Json (Join-Path $repairOutput 'build.json') ([ordered]@{
    release=$patch.release; candidate=$patch.candidate;
    originalDllSha256=$patch.originalSha256; patchedDllSha256=$patch.patchedSha256;
    recipeSha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'candidate-patch.json')).Hash.ToLowerInvariant();
    sourcePatchSha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'source-port.patch')).Hash.ToLowerInvariant();
    completeEntranceSourcePort=$false; gameplayVerified=$false
})
foreach ($file in $release.files) {
    $path = Child-Path $outputPayload $file.path
    if ((Get-Item -LiteralPath $path).Length -ne $file.size -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) { throw "Output validation failed: $($file.path)" }
}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zipPath, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in Get-ChildItem -LiteralPath $outputRoot -File -Recurse) {
        $relative = $file.FullName.Substring($outputRoot.Length).TrimStart('\', '/').Replace('\', '/')
        $entryName = [IO.Path]::GetFileName($outputRoot) + '/' + $relative
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entryName, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose() }
[pscustomobject]@{ release=$release.release; directory=$outputRoot; zip=$zipPath; dllSha256=$patch.patchedSha256 }
