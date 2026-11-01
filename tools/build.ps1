# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
[CmdletBinding()]
param([switch]$Run, [ValidateSet('Debug','Release')][string]$Configuration='Release')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$buildArguments=@((Join-Path $PSScriptRoot 'build.py'),'--configuration',$Configuration)
if ($Run) { $buildArguments+='--run' }
& python @buildArguments
if ($LASTEXITCODE -ne 0) { throw 'Compilation ou verification impossible.' }

