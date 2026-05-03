#!/bin/bash
# Script to help diagnose USB serial connection issues for ESP32 on WSL.

echo "--- Starting USB Serial Diagnosis ---"

# Check if any typical serial device files exist
if ls /dev/ttyACM* /dev/ttyUSB* 1> /dev/null 2>&1; then
    echo "Success: Found potential serial devices in /dev/."
else
    echo "Warning: No standard serial devices (ttyACM*, ttyUSB*) found. This is common if the board isn't connected or detected correctly."
fi

# List all available USB devices to check for chip identifiers
echo ""
echo "--- Listing all connected USB devices (lsusb) ---"
lsusb

echo ""
echo "--- Done diagnosing serial ports. Please check dmesg and permissions if issues persist. ---"