param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectName,

    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$PackageDir,

    [Parameter(Mandatory = $true)]
    [string]$PackageZip
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -Path $Executable -PathType Leaf)) {
    throw "Release executable not found: $Executable"
}

if (-not (Test-Path -Path 'assets' -PathType Container)) {
    throw 'Runtime assets directory not found: assets'
}

Remove-Item -Path $PackageDir -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path $PackageZip -Force -ErrorAction SilentlyContinue

New-Item -Path $PackageDir -ItemType Directory -Force | Out-Null
Copy-Item -Path $Executable -Destination $PackageDir
Copy-Item -Path 'assets' -Destination $PackageDir -Recurse
Copy-Item -Path 'CREDITS.md' -Destination $PackageDir

Compress-Archive -Path "$PackageDir\*" -DestinationPath $PackageZip -CompressionLevel Optimal

Write-Host "Created itch.io package: $PackageZip"