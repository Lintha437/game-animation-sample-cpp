[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE_ROOT,
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $VerifyOnly) {
    if (-not $EngineRoot) {
        throw 'Pass -EngineRoot or set UE_ROOT to your Unreal Engine 5.9 installation.'
    }
    $project = Join-Path $repositoryRoot 'GameAnimSampleCpp.uproject'
    $engine = Join-Path (Resolve-Path -LiteralPath $EngineRoot).Path 'Engine'
    $version = Get-Content -Raw -LiteralPath (Join-Path $engine 'Build/Build.version') | ConvertFrom-Json
    if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 9) {
        throw 'This project requires Unreal Engine 5.9.'
    }

    $ubt = Join-Path $engine 'Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe'
    & $ubt -ProjectFiles "-Project=$project" -Game -Engine -2026
    if ($LASTEXITCODE -ne 0) {
        throw "Unreal Build Tool returned exit code $LASTEXITCODE."
    }
}

$solutions = @(
    'GameAnimSampleCpp.sln',
    'GameAnimSampleCpp.slnx',
    'Automation_GameAnimSampleCpp.sln',
    'Automation_GameAnimSampleCpp.slnx'
)

foreach ($name in $solutions) {
    $path = Join-Path $repositoryRoot $name
    $text = Get-Content -Raw -LiteralPath $path
    if ($name.EndsWith('.slnx')) {
        $document = [xml]$text
        $references = $document.SelectNodes('//Project[@Path]') | ForEach-Object { $_.GetAttribute('Path') }
    }
    else {
        $references = [regex]::Matches($text, '(?im)^Project\("[^"]+"\)\s*=\s*"[^"]+",\s*"([^"]+proj)"') |
            ForEach-Object { $_.Groups[1].Value }
    }

    $expected = if ($name.StartsWith('Automation_')) { 'AutomationUtils.Automation.csproj' } else { 'GameAnimSampleCpp.vcxproj' }
    if (-not ($references | Where-Object { [IO.Path]::GetFileName($_) -eq $expected })) {
        throw "$name does not reference $expected."
    }

    foreach ($reference in $references) {
        $fullPath = [IO.Path]::GetFullPath([IO.Path]::Combine($repositoryRoot, $reference))
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            throw "$name references a missing project at $fullPath."
        }
    }
    Write-Output $path
}

$nativeProject = Join-Path $repositoryRoot 'Intermediate/ProjectFiles/GameAnimSampleCpp.vcxproj'
$projectEntries = @(
    'GameAnimationSampleCpp.Target.cs',
    'GameAnimationSampleCppEditor.Target.cs',
    'SampleMoverCharacter.cpp',
    'SampleLocomotionCharacter.cpp',
    'SampleLocomotionAnimInstance.cpp',
    'SampleTraversalProbeComponent.cpp'
)
foreach ($file in $projectEntries) {
    if (-not (Select-String -LiteralPath $nativeProject -SimpleMatch $file -Quiet)) {
        throw "$nativeProject is missing $file."
    }
}
