#!/bin/bash

# Simple build test script for RealMesh
echo "=== RealMesh Build Test ==="

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    echo "Error: Not in RealMesh project directory"
    exit 1
fi

echo "Project directory: $(pwd)"
echo "Build timestamp: $(date)"

# Try to find PlatformIO
if command -v pio &> /dev/null; then
    echo "Found PlatformIO CLI"
    pio run
elif [ -f "/Applications/Visual Studio Code.app/Contents/Resources/app/extensions/platformio.platformio-ide-*/penv/bin/pio" ]; then
    echo "Found PlatformIO via VS Code"
    "/Applications/Visual Studio Code.app/Contents/Resources/app/extensions/platformio.platformio-ide-"*/penv/bin/pio run
else
    echo "PlatformIO not found. Please:"
    echo "1. Install PlatformIO IDE extension in VS Code"
    echo "2. Use VS Code's build button (checkmark in bottom toolbar)"
    echo "3. Or install PlatformIO CLI globally"
    exit 1
fi

echo "=== Build Complete ==="