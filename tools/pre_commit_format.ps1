$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel).Trim()
if (-not $repoRoot) {
    throw 'Unable to determine repository root.'
}

Set-Location $repoRoot

$stagedFiles = @(git diff --cached --name-only --diff-filter=ACMR)
if ($stagedFiles.Count -eq 0) {
    exit 0
}

$existingFiles = $stagedFiles |
Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }

$dartFiles = @(
    $existingFiles |
    Where-Object { $_ -match '\.dart$' }
)

$cppFiles = @(
    $existingFiles |
    Where-Object { $_ -match '\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$' }
)

if ($dartFiles.Count -gt 0) {
    & dart format @dartFiles
}

if ($cppFiles.Count -gt 0) {
    & clang-format -i @cppFiles
}

if ($dartFiles.Count -gt 0 -or $cppFiles.Count -gt 0) {
    git add -- @($dartFiles + $cppFiles)
}
