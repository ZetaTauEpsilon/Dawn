[CmdletBinding()]
param([Parameter(Mandatory)][string]$CandidateRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$builder = Join-Path $PSScriptRoot 'New-1AUNoWipeCandidate.ps1'
$recipe = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'candidate-patch.json') -Raw | ConvertFrom-Json
$testRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ('..\..\build\unit\one-au-no-wipe-' + [guid]::NewGuid().ToString('N'))))
$output = Join-Path $testRoot 'Dawn-0.1.3-1au-no-wipe'
$originalDll = Join-Path $CandidateRoot 'payload\steam_api64.dll'
$originalHash = (Get-FileHash -LiteralPath $originalDll -Algorithm SHA256).Hash
$built = & $builder -CandidateRoot $CandidateRoot -OutputDirectory $output
$manifest = Get-Content -LiteralPath (Join-Path $output 'release.json') -Raw | ConvertFrom-Json
if ($manifest.release -ne $recipe.release) { throw 'Wrong output release.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($built.zip)
try {
    foreach ($file in $manifest.files) {
        $path = Join-Path (Join-Path $output 'payload') $file.path
        if ((Get-FileHash -LiteralPath $path).Hash -ne $file.sha256) { throw 'Output manifest mismatch.' }
        if ($file.path -ne 'steam_api64.dll' -and (Get-FileHash -LiteralPath (Join-Path (Join-Path $CandidateRoot 'payload') $file.path)).Hash -ne $file.sha256) { throw 'Unrelated payload changed.' }
        $entry = $zip.GetEntry('Dawn-0.1.3-1au-no-wipe/payload/' + $file.path.Replace('\', '/'))
        if ($null -eq $entry -or $entry.Length -ne $file.size) { throw "Missing ZIP payload: $($file.path)" }
        $stream = $entry.Open(); $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose(); $stream.Dispose() }
        if ($hash -ne $file.sha256) { throw 'ZIP payload hash mismatch.' }
    }
} finally { $zip.Dispose() }
if ((Get-FileHash -LiteralPath $originalDll).Hash -ne $originalHash) { throw 'Input DLL changed.' }
function Expect-Failure([string]$Expected, [scriptblock]$Action) {
    $failed = $false
    try { & $Action | Out-Null } catch { if ($_.Exception.Message -notlike $Expected) { throw }; $failed = $true }
    if (-not $failed) { throw "Expected rejection: $Expected" }
}
Expect-Failure 'Output already exists*' { & $builder -CandidateRoot $CandidateRoot -OutputDirectory $output }
Expect-Failure 'Output must be separate*' { & $builder -CandidateRoot $CandidateRoot -OutputDirectory (Join-Path $CandidateRoot 'nested-output') }
$different = Join-Path $testRoot 'different-build'
Copy-Item -LiteralPath $output -Destination $different -Recurse
$manifestPath = Join-Path $different 'release.json'
$changed = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$changed.release = $recipe.candidate
[IO.File]::WriteAllText($manifestPath, ($changed | ConvertTo-Json -Depth 10))
$rejected = Join-Path $testRoot 'must-not-exist'
Expect-Failure 'Unsupported DLL*' { & $builder -CandidateRoot $different -OutputDirectory $rejected }
if (Test-Path -LiteralPath $rejected) { throw 'Rejected input produced an output.' }
[pscustomobject]@{ payloadFilesVerified=@($manifest.files).Count; allZipPayloadHashesVerified=$true;
    originalInputPreserved=$true; otherPayloadPreserved=$true; existingOutputRejected=$true;
    nestedOutputRejected=$true; differentDllRejectedBeforeWriting=$true } | ConvertTo-Json
