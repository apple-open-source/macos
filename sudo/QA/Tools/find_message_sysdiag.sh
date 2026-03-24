#!/bin/bash

# Script to analyze sysdiagnose logarchive for specific messages
# Usage: ./analyze_sysdiagnose.sh <path_to_sysdiagnose.tar.gz> <search_string>

set -e

if [ $# -lt 2 ]; then
    echo "Usage: $0 <sysdiagnose.tar.gz> <search_string>"
    echo "Example: $0 ~/Downloads/sysdiagnose_2025.10.15.tar.gz 'Reading managed config'"
    exit 1
fi

SYSDIAGNOSE_FILE="$1"
SEARCH_STRING="$2"

# Check if file exists
if [ ! -f "$SYSDIAGNOSE_FILE" ]; then
    echo "Error: File not found: $SYSDIAGNOSE_FILE"
    exit 1
fi

# Create temporary directory for extraction
TEMP_DIR=$(mktemp -d -t sysdiagnose_analysis)
echo "Extracting archive to: $TEMP_DIR"

# Extract the archive
tar -xzf "$SYSDIAGNOSE_FILE" -C "$TEMP_DIR"

# Find the logarchive
LOGARCHIVE=$(find "$TEMP_DIR" -name "system_logs.logarchive" -type d | head -1)

if [ -z "$LOGARCHIVE" ]; then
    echo "Error: Could not find system_logs.logarchive in the extracted archive"
    rm -rf "$TEMP_DIR"
    exit 1
fi

echo "Found logarchive at: $LOGARCHIVE"
echo "Searching for: '$SEARCH_STRING'"
echo "----------------------------------------"

# Search the logarchive
RESULTS=$(/usr/bin/log show --predicate "eventMessage CONTAINS \"$SEARCH_STRING\"" "$LOGARCHIVE")

# Check if results were found
if echo "$RESULTS" | grep -q "^[0-9]"; then
    echo "$RESULTS"
    echo "----------------------------------------"
    COUNT=$(echo "$RESULTS" | grep -c "^[0-9]" || true)
    echo "Found $COUNT matching entries"
else
    echo "No results found for: '$SEARCH_STRING'"
fi

# Cleanup
echo "Cleaning up temporary directory..."
rm -rf "$TEMP_DIR"
echo "Done!"
