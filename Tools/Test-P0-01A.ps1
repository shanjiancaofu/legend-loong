#Requires -Version 7.0
# Backwards-compatible entry; the shared runner discovers the engine and verifies exact test discovery.
param([string]$EngineRoot, [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'Run-LoongTests.ps1') -Group Full -EngineRoot $EngineRoot -SkipBuild:$SkipBuild
