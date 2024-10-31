# scripts/build.ps1

# Ensure we stop on any error
$ErrorActionPreference = "Stop"

# Function to check if a command exists
function Test-Command($cmdname) {
    return [bool](Get-Command -Name $cmdname -ErrorAction SilentlyContinue)
}

# Check if required tools are installed
if (-not (Test-Command "conan")) {
    Write-Error "Conan is not installed or not in PATH. Please install Conan and try again."
    exit 1
}

if (-not (Test-Command "cmake")) {
    Write-Error "CMake is not installed or not in PATH. Please install CMake and try again."
    exit 1
}

# Get the project root directory (parent of the scripts directory)
$projectDir = (Get-Item $PSScriptRoot).Parent.FullName

# Create and navigate to build directory
$buildDir = Join-Path $projectDir "build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Set-Location $buildDir

# Step 1: Run Conan install
Write-Host "Running Conan install..."
conan install $projectDir --build=missing -s compiler.cppstd=20

if ($LASTEXITCODE -ne 0) {
    Write-Error "Conan install failed. Please check the error messages above."
    exit 1
}

# Step 2: Configure with CMake
Write-Host "Configuring with CMake..."
cmake $projectDir -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=.\generators\conan_toolchain.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed. Please check the error messages above."
    exit 1
}

# Step 3: Build the project
Write-Host "Building the project..."
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed. Please check the error messages above."
    exit 1
}

# Step 4: Copy all files from the Release folder to the output directory
Write-Host "Copying files to output directory..."
$outputDir = Join-Path $projectDir "output"
$releasePath = Join-Path $buildDir "Release"

if (-not (Test-Path $releasePath)) {
    Write-Error "Release folder not found at expected location: $releasePath"
    exit 1
}

# Create the output directory if it doesn't exist
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

# Copy all files from the Release folder to the output directory
Copy-Item -Path "$releasePath\*" -Destination $outputDir -Recurse -Force

Write-Host "Build completed successfully. All files from Release folder copied to $outputDir"

# Return to the original directory
Set-Location $PSScriptRoot
