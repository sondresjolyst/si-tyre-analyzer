# SI Tyre Analyzer installer (Windows).
# Installs uv if missing, installs the app from the latest GitHub release,
# and creates a desktop shortcut.
#Requires -Version 5
$ErrorActionPreference = "Stop"
$Repo = "sondresjolyst/si-tyre-analyzer"

if (-not (Get-Command uv -ErrorAction SilentlyContinue)) {
  Write-Host "Installing uv..."
  Invoke-RestMethod https://astral.sh/uv/install.ps1 | Invoke-Expression
  $env:Path = "$env:USERPROFILE\.local\bin;$env:Path"
}

Write-Host "Finding the latest release..."
$releases = Invoke-RestMethod "https://api.github.com/repos/$Repo/releases" `
  -Headers @{ "User-Agent" = "sita-installer" }
$release = $releases |
  Where-Object { $_.tag_name -like "si-tyre-analyzer-app-v*" -and -not $_.prerelease -and -not $_.draft } |
  Select-Object -First 1
if (-not $release) { throw "No app release found on GitHub." }
$asset = $release.assets | Where-Object { $_.name -like "*.whl" } | Select-Object -First 1
if (-not $asset) { throw "The release has no .whl to install." }

$tmp = Join-Path $env:TEMP $asset.name
Write-Host "Downloading $($asset.name)..."
Invoke-WebRequest $asset.browser_download_url -OutFile $tmp

Write-Host "Installing..."
uv tool install --force $tmp

$exe = (Get-Command si-tyre-analyzer -ErrorAction SilentlyContinue).Source
if ($exe) {
  $ico = Join-Path $env:LOCALAPPDATA "si-tyre-analyzer\app.ico"
  & sita make-icon $ico | Out-Null

  $lnk = Join-Path ([Environment]::GetFolderPath("Desktop")) "SI Tyre Analyzer.lnk"
  $shell = New-Object -ComObject WScript.Shell
  $shortcut = $shell.CreateShortcut($lnk)
  $shortcut.TargetPath = $exe
  if (Test-Path $ico) { $shortcut.IconLocation = $ico }
  $shortcut.Save()
  Write-Host "Created desktop shortcut: $lnk"
}
Write-Host "Done. Launch 'SI Tyre Analyzer' from your desktop."
