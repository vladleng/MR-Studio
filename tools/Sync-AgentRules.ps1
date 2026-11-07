<#
.SYNOPSIS
    Publishes local agent rules (AGENTS.md and .agents/skills) to GitHub main.

.DESCRIPTION
    Local AGENTS.md is the source of truth (user decision 2026-11-07). This script copies the
    local AGENTS.md and .agents/skills tree into a temporary worktree of origin/main, stages only
    those paths and (with -Yes) pushes a docs-only commit to main with a [skip ci] suffix.

    Nothing else is published: no source code, no builds, no user files, no docs outside the
    listed paths.

.PARAMETER Message
    Commit message subject. "[skip ci]" is appended automatically when missing.

.PARAMETER DryRun
    Only create the worktree, apply local files and print the pending diff; never commit or push.

.PARAMETER Yes
    Necessary for the actual push. Without it the script behaves like -DryRun plus a hint.

.EXAMPLE
    pwsh -ExecutionPolicy Bypass -File tools/Sync-AgentRules.ps1 -DryRun

.EXAMPLE
    pwsh -ExecutionPolicy Bypass -File tools/Sync-AgentRules.ps1 -Yes -Message "docs(agents): clarify local build policy"
#>
[CmdletBinding()]
param(
    [string]$Message = 'docs(agents): sync local agent rules',
    [switch]$DryRun,
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'

function Invoke-Git {
    param([string[]]$Arguments, [switch]$AllowFailure)
    # Native git progress goes to stderr; Windows PowerShell 5.1 would raise it as a
    # terminating error under ErrorActionPreference=Stop, so relax it for the call only.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & git @Arguments 2>&1
        $code = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0 -and -not $AllowFailure) {
        throw ("git {0} failed:`n{1}" -f ($Arguments -join ' '), ($output -join "`n"))
    }
    return $output
}

$repoRoot = (& git rev-parse --show-toplevel).Trim()
if (-not $repoRoot) { throw 'Not inside a git repository.' }
Push-Location $repoRoot
$worktree = $null

try {
    $paths = @('AGENTS.md', '.agents/skills')

    Write-Host "Fetching origin/main..."
    Invoke-Git @('fetch', 'origin', 'main', '--quiet') | Out-Null

    $worktree = Join-Path ([System.IO.Path]::GetTempPath()) ("mrs-agentrules-" + [guid]::NewGuid().ToString('N'))
    Write-Host "Creating temporary worktree for origin/main..."
    Invoke-Git @('worktree', 'add', '--detach', $worktree, 'origin/main') | Out-Null

    foreach ($path in $paths) {
        $source = Join-Path $repoRoot $path
        if (-not (Test-Path -LiteralPath $source)) { throw "Missing local path: $path" }
        $target = Join-Path $worktree $path
        if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
        Copy-Item -LiteralPath $source -Destination $target -Recurse -Force
        if (Test-Path -LiteralPath (Join-Path $worktree $path)) { } else { throw "Copy failed: $path" }
    }

    Invoke-Git @('-C', $worktree, 'add', '--', 'AGENTS.md', '.agents/skills') | Out-Null

    $staged = (& git -C $worktree diff --cached --name-only) | Where-Object { $_ }
    if (-not $staged) {
        Write-Host 'Nothing to publish: origin/main already matches local AGENTS.md and .agents/skills.'
        return
    }

    Write-Host ''
    Write-Host 'Pending docs-only changes on main:'
    foreach ($file in $staged) { Write-Host "  $file" }
    Write-Host ''
    & git -C $worktree diff --cached --stat

    if ($DryRun -or -not $Yes) {
        Write-Host ''
        if (-not $DryRun) { Write-Host 'Dry run only. Re-run with -Yes to publish to main.' }
        Write-Host 'Nothing was committed or pushed.'
        return
    }

    $subject = $Message.Trim()
    if ($subject -notmatch '\[skip ci\]') { $subject = "$subject [skip ci]" }

    Invoke-Git @('-C', $worktree, 'commit', '-m', $subject) | Out-Null
    Write-Host "Pushing docs-only commit to origin/main: $subject"
    $pushed = Invoke-Git @('-C', $worktree, 'push', 'origin', 'HEAD:main')
    Write-Host ($pushed -join "`n")
    Write-Host 'Published. Published files:'
    foreach ($file in $staged) { Write-Host "  $file" }
}
finally {
    if ($worktree -and (Test-Path -LiteralPath $worktree)) {
        & git -C $repoRoot worktree remove --force $worktree 2>&1 | Out-Null
    }
    Pop-Location
}
