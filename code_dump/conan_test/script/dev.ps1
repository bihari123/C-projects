# Get Conan home directory
$CONAN_HOME = conan config home

# Define destination directories
$LIB_DEST = "C:\Program Files\conan-libs"
$INCLUDE_DEST = "C:\Program Files\conan-includes"

# Create destination directories if they don't exist
New-Item -ItemType Directory -Force -Path $LIB_DEST
New-Item -ItemType Directory -Force -Path $INCLUDE_DEST

# Function to recursively search for files
function Find-Files {
    param (
        [string]$Path,
        [string[]]$Include
    )
    Get-ChildItem -Path $Path -Recurse -Include $Include -ErrorAction SilentlyContinue
}

# Copy libraries
Find-Files -Path $CONAN_HOME -Include @("*.lib", "*.dll") | ForEach-Object {
    Copy-Item $_.FullName -Destination $LIB_DEST -Force
    Write-Host "Copied $($_.Name) to $LIB_DEST"
}

# Copy headers
Find-Files -Path $CONAN_HOME -Include @("*.h", "*.hpp") | ForEach-Object {
    $relativePath = $_.FullName.Substring($CONAN_HOME.Length)
    $destination = Join-Path $INCLUDE_DEST $relativePath
    $destinationDir = Split-Path -Parent $destination
    if (-not (Test-Path $destinationDir)) {
        New-Item -ItemType Directory -Force -Path $destinationDir | Out-Null
    }
    Copy-Item $_.FullName -Destination $destination -Force
    Write-Host "Copied $($_.Name) to $destination"
}

Write-Host "Installation complete. Libraries copied to $LIB_DEST and headers to $INCLUDE_DEST"
