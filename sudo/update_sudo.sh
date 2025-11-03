#!/bin/zsh
#
# Script: update_sudo.sh
# Description: Updates sudo to a new version by applying patches from the current version
# Usage: ./update_sudo.sh <new_sudo_version>
#

# Exit on any error, but print the command that failed
set -e
set -o pipefail

# Enable debug mode to see commands being executed
# Uncomment the line below for debugging
# set -x

# Function to print error messages and exit
error_exit() {
    echo "ERROR: $1" >&2
    exit 1
}

# Global flag to ignore SHA checksum mismatches
ignore_sha=false

# Function to print status messages
print_status() {
    echo "===== $1 ====="
}

# Function to configure sudo for PR
configure_sudo() {
    print_status "Configuring sudo"
    
    # Check if sudo directory exists
    if [[ ! -d "sudo" ]]; then
        error_exit "sudo directory not found in the current directory"
    fi
    
    # Run configure in the sudo directory
    print_status "Running configure in sudo directory"
    (cd sudo && xcrun --sdk macosx.internal --run $PWD/configure --with-password-timeout=0 --disable-setreuid --with-env-editor --with-pam --with-libraries=bsm --with-noexec=no --sysconfdir="/private/etc" --without-lecture --enable-static-sudoers --with-rundir="/var/db/sudo") || error_exit "Failed to run configure in sudo directory"
    
    # Modify config.h to apply Apple-specific build requirements
    print_status "Modifying sudo/config.h for Apple-specific build"
    if [[ -f "sudo/config.h" ]]; then
        # Check if HAVE_OPENSSL is defined and undefine it
        if grep -q "#define HAVE_OPENSSL" "sudo/config.h"; then
            print_status "Undefining HAVE_OPENSSL in config.h (Apple-specific requirement)"
            # Add a comment explaining the change and then undefine HAVE_OPENSSL
            sed -i '' 's/#define HAVE_OPENSSL 1/\/* HAVE_OPENSSL is explicitly disabled for Apple-specific build *\/\n/* #undef HAVE_OPENSSL/*\/' "sudo/config.h"
        fi
        
        # Check if HAVE_UTIL_H is not defined and define it
        if ! grep -q "#define HAVE_UTIL_H" "sudo/config.h"; then
            print_status "Defining HAVE_UTIL_H in config.h (Apple-specific requirement)"
            # Add the definition with a comment before the end of the file
            sed -i '' '$ i\\
\/* HAVE_UTIL_H is explicitly enabled for Apple-specific build *\/\\
#define HAVE_UTIL_H 1
' "sudo/config.h"
        fi
    else
        print_status "Warning: sudo/config.h not found, skipping modifications"
    fi
    
    # Build man pages
    print_status "Building man pages in sudo/docs directory"
    (cd sudo/docs && make sudo.man sudoers.man visudo.man sudo.conf.man sudo_plugin.man sudoers_timestamp.man sudoreplay.man) || error_exit "Failed to build man pages"
    
    # Remove generated Makefiles but keep Makefile.in files
    print_status "Removing generated Makefiles from sudo directory"
    find sudo -name "Makefile" -type f -delete || print_status "Warning: Failed to remove some Makefile occurrences"
    
    print_status "Sudo configuration completed"
}

# Function to create diff between current sudo and vanilla version
create_diff() {
    local vanillaFolder="$1"
    local diffFile="$2"
    
    print_status "Creating diff between current sudo and vanilla version"

    # Create a temporary directory for diff creation with relative paths
    tempDiffDir="$workingFolder/diff-temp"
    mkdir -p "$tempDiffDir"

    # Copy vanilla folder and sudo to temp directory with relative names
    vanillaBasename=$(basename "$vanillaFolder")
    cp -R "$vanillaFolder" "$tempDiffDir/$vanillaBasename"
    cp -R "sudo" "$tempDiffDir/sudo"

    # Create diff using relative paths
    (cd "$tempDiffDir" && diff -Naur "$vanillaBasename" "sudo" > "$workingFolder/diff-temp.diff") || true  # diff returns non-zero if files differ, which is expected
    mv "$workingFolder/diff-temp.diff" "$diffFile"
    print_status "Diff file created with relative paths: $diffFile"
}

# Function to apply patch to sudo
apply_patch() {
    local diffFile="$1"
    local targetDir="$2"
    local fullPath=$(cd "$targetDir" && pwd)
    
    print_status "Applying patch to $fullPath"
    print_status "Using patch file: $diffFile"
    patchSuccess=false
    
    # Create a temporary file to capture patch output
    patchOutput=$(mktemp)
    
    # Try to apply the patch with different strip levels
    for p in 0 1 2 3; do
        print_status "Trying patch with -p$p on $fullPath"
        
        # Apply the patch with basic options compatible with BSD patch
        patch -p$p -d "$targetDir" -f < "$diffFile" > "$patchOutput" 2>&1
        patchStatus=$?
        
        # Show the output of the patch command
        cat "$patchOutput"
        
        if [[ $patchStatus -eq 0 ]]; then
            patchSuccess=true
            print_status "Patch applied successfully to $fullPath with -p$p"
            
            # Extract missing files from the output using grep
            print_status "Checking for missing files..."
            grep -o "[^:]*: No such file or directory" "$patchOutput" 2>/dev/null | sed 's/: No such file or directory//' > "$workingFolder/missing_files.txt" 2>/dev/null || true
            
            # Print list of missing files if any
            if [[ -s "$workingFolder/missing_files.txt" ]]; then
                print_status "The following files were skipped because they were missing:"
                cat "$workingFolder/missing_files.txt" | while read file; do
                    echo "  - $file"
                done
            else
                print_status "No missing files detected."
            fi
            
            break
        else
            print_status "Patch failed to apply with -p$p. Error code: $patchStatus"
            print_status "See output above for details."
        fi
    done
    
    # Clean up temporary file
    rm -f "$patchOutput"
    
    if $patchSuccess; then
        return 0
    else
        print_status "Patch failed to apply with all strip levels."
        print_status "Please check the output above for detailed error messages."
        return 1
    fi
}

# Function to apply patch to sudo (legacy function, kept for compatibility)
apply_patch_to_sudo() {
    local diffFile="$1"
    local patchedFolder="$2"
    local newSudoVersion="$3"
    local currentSudoVersion="$4"
    local fullPath=$(cd "$patchedFolder" && pwd)
    
    print_status "Applying patch to new sudo version"
    print_status "Diff $diffFile patched $fullPath"
    patchSuccess=false
    if patch -p0 -d "$patchedFolder" -f < "$diffFile"; then
        patchSuccess=true
        print_status "Patch applied successfully"
        
        # Rename current sudo directory to sudo-$currentSudoVersion
        print_status "Renaming current sudo directory to sudo-$currentSudoVersion"
        if [[ -d "sudo-$currentSudoVersion" ]]; then
            print_status "Directory sudo-$currentSudoVersion already exists, removing it"
            rm -rf "sudo-$currentSudoVersion"
        fi
        mv "sudo" "sudo-$currentSudoVersion" || error_exit "Failed to rename sudo directory"
        
        # Copy patched folder to current directory as "sudo"
        print_status "Copying patched folder to current directory as sudo"
        cp -R "$patchedFolder" "sudo" || error_exit "Failed to copy patched folder"
        
        print_status "Successfully updated sudo to version $newSudoVersion"
        return 0
    else
        print_status "Patch failed to apply. No files will be copied."
        return 1
    fi
}

# Function to display help information
display_help() {
    echo "Script: update_sudo.sh"
    echo "Description: Updates sudo to a new version by applying patches from the current version"
    echo ""
    echo "Usage:"
    echo "  $0 <new_sudo_version>  - Update sudo to the specified version"
    echo "  $0 --configure         - Only configure sudo and build man pages"
    echo "  $0 --patch             - Apply the patch to the current directory/sudo"
    echo "  $0 --make-diff         - Only create the diff file"
    echo "  $0 --ignore-sha        - Ignore SHA checksum mismatches when downloading"
    echo "  $0 --installsrc <new_sudo_version> - Download files, create diff, backup old sudo, and copy new vanilla sudo"
    echo "  $0 --installsrc --ignore-sha <new_sudo_version> - Install source and ignore SHA checksum mismatches"
    echo "  $0 -h, --help          - Display this help message"
    echo ""
    exit 0
}

# Function to trap errors and print more information
trap_error() {
    local line=$1
    local command=$2
    local code=$3
    echo "ERROR: Command '$command' failed with exit code $code at line $line"
    exit $code
}

# Set up error trap
trap 'trap_error ${LINENO} "$BASH_COMMAND" $?' ERR

# Check for help, configure, patch, or ignore-sha flags
if [[ $# -ge 1 ]]; then
    case "$1" in
        --configure)
            print_status "Running in configure-only mode"
            configure_sudo
            exit 0
            ;;
        --installsrc)
            print_status "Running in install-source-only mode"
            
            # Check for --ignore-sha flag
            if [[ $# -ge 3 && "$2" == "--ignore-sha" ]]; then
                print_status "SHA checksum verification will be ignored"
                ignore_sha=true
                # Get the new sudo version from the third parameter
                newSudoVersion="$3"
                if [[ -z "$newSudoVersion" ]]; then
                    error_exit "Usage: $0 --installsrc --ignore-sha <new_sudo_version>"
                fi
            elif [[ $# -ne 2 ]]; then
                error_exit "Usage: $0 --installsrc <new_sudo_version> or $0 --installsrc --ignore-sha <new_sudo_version>"
            else
                # Get the new sudo version from the second parameter
                newSudoVersion="$2"
            fi
            
            print_status "Installing source for sudo version $newSudoVersion"
            
            # Check if sudo.plist exists in the current directory
            if [[ ! -f "sudo.plist" ]]; then
                error_exit "sudo.plist not found in the current directory"
            fi
            
            print_status "Reading sudo.plist file"
            # Extract OpenSourceVersion (current sudo version)
            currentSudoVersion=$(plutil -extract OpenSourceVersion raw -o - sudo.plist)
            if [[ -z "$currentSudoVersion" ]]; then
                error_exit "Failed to extract OpenSourceVersion from sudo.plist"
            fi
            print_status "Current sudo version: $currentSudoVersion"
            
            # Create working folder
            workingFolder="/var/tmp/sudo-installsrc-$newSudoVersion"
            
            # Remove working folder if it exists from previous run
            if [[ -d "$workingFolder" ]]; then
                print_status "Removing existing working folder from previous run"
                rm -rf "$workingFolder" || error_exit "Failed to remove existing working folder"
            fi
            
            print_status "Creating working folder: $workingFolder"
            mkdir -p "$workingFolder" || error_exit "Failed to create working folder"
            mkdir -p "$workingFolder/download" || error_exit "Failed to create download folder"
            
            # Extract OpenSourceURL
            sourceURL=$(plutil -extract OpenSourceURL raw -o - sudo.plist)
            if [[ -z "$sourceURL" ]]; then
                error_exit "Failed to extract OpenSourceURL from sudo.plist"
            fi
            print_status "Source URL: $sourceURL"
            
            # Extract OpenSourceSHA256
            sourceSHA256=$(plutil -extract OpenSourceSHA256 raw -o - sudo.plist)
            if [[ -z "$sourceSHA256" ]]; then
                error_exit "Failed to extract OpenSourceSHA256 from sudo.plist"
            fi
            print_status "Source SHA256: $sourceSHA256"
            
            # Download the current version
            currentVersionFile="$workingFolder/download/sudo-$currentSudoVersion.tar.gz"
            print_status "Downloading current sudo version from $sourceURL"
            curl -L "$sourceURL" -o "$currentVersionFile" || error_exit "Failed to download current sudo version"
            
            # Verify SHA256 checksum
            print_status "Verifying SHA256 checksum"
            calculatedSHA256=$(shasum -a 256 "$currentVersionFile" | awk '{print $1}')
            if [[ "$calculatedSHA256" != "$sourceSHA256" ]]; then
                if $ignore_sha; then
                    print_status "SHA256 checksum mismatch ignored. Expected: $sourceSHA256, Got: $calculatedSHA256"
                else
                    error_exit "SHA256 checksum verification failed. Expected: $sourceSHA256, Got: $calculatedSHA256"
                fi
            else
                print_status "SHA256 checksum verified successfully"
            fi
            
            # Unpack the current version
            vanillaFolder="$workingFolder/sudo-vanilla-$currentSudoVersion"
            print_status "Unpacking current sudo version to $vanillaFolder"
            mkdir -p "$vanillaFolder" || error_exit "Failed to create vanilla folder"
            tar -xzf "$currentVersionFile" -C "$vanillaFolder" --strip-components=1 || error_exit "Failed to unpack current sudo version"
            
            # Check if sudo directory exists in the current directory
            if [[ ! -d "sudo" ]]; then
                error_exit "sudo directory not found in the current directory"
            fi
            
            # Create diff between current sudo and vanilla version
            diffFile="$workingFolder/sudo-patch-$currentSudoVersion.diff"
            create_diff "$vanillaFolder" "$diffFile"
            
            # Create URL for the new version
            newSourceURL="${sourceURL/$currentSudoVersion/$newSudoVersion}"
            print_status "New source URL: $newSourceURL"
            
            # Download the new version
            newVersionFile="$workingFolder/download/sudo-$newSudoVersion.tar.gz"
            print_status "Downloading new sudo version from $newSourceURL"
            curl -L "$newSourceURL" -o "$newVersionFile" || error_exit "Failed to download new sudo version"
            
            # Calculate SHA256 for the new version
            newSourceSHA256=$(shasum -a 256 "$newVersionFile" | awk '{print $1}')
            print_status "New source SHA256: $newSourceSHA256"
            
            # Unpack the new version
            newVanillaFolder="$workingFolder/sudo-vanilla-$newSudoVersion"
            print_status "Unpacking new sudo version to $newVanillaFolder"
            mkdir -p "$newVanillaFolder" || error_exit "Failed to create new vanilla folder"
            tar -xzf "$newVersionFile" -C "$newVanillaFolder" --strip-components=1 || error_exit "Failed to unpack new sudo version"
            
            # Rename current sudo directory to sudo-$currentSudoVersion
            print_status "Renaming current sudo directory to sudo-$currentSudoVersion"
            if [[ -d "sudo-$currentSudoVersion" ]]; then
                print_status "Directory sudo-$currentSudoVersion already exists, removing it"
                rm -rf "sudo-$currentSudoVersion"
            fi
            mv "sudo" "sudo-$currentSudoVersion" || error_exit "Failed to rename sudo directory"
            
            # Copy new vanilla folder to current directory as "sudo"
            print_status "Copying new vanilla version ($newVanillaFolder) to current directory as sudo"
            cp -R "$newVanillaFolder" "sudo" || error_exit "Failed to copy new vanilla version"
            
            # Update sudo.plist with new version information
            print_status "Updating sudo.plist with new version information"
            # Get current date in the format YYYY-MM-DD
            currentDate=$(date +"%Y-%m-%d")
            
            # Create a temporary file with the updated values
            tempPlist=$(mktemp)
            plutil -convert xml1 sudo.plist -o "$tempPlist"
            
            # Update OpenSourceVersion
            plutil -replace OpenSourceVersion -string "$newSudoVersion" "$tempPlist"
            
            # Update OpenSourceURL
            plutil -replace OpenSourceURL -string "$newSourceURL" "$tempPlist"
            
            # Update OpenSourceSHA256
            plutil -replace OpenSourceSHA256 -string "$newSourceSHA256" "$tempPlist"
            
            # Update OpenSourceImportDate
            plutil -replace OpenSourceImportDate -string "$currentDate" "$tempPlist"
            
            # Replace the original file
            mv "$tempPlist" sudo.plist
            
            print_status "Installation of source completed successfully"
            print_status "You can now run '$0 --configure' to configure sudo"
            print_status "Then run '$0 --patch' to apply the patch"
            exit 0
            ;;
        --patch)
            print_status "Running in patch-only mode"
            
            # Check if sudo directory exists in the current directory
            if [[ ! -d "sudo" ]]; then
                error_exit "sudo directory not found in the current directory"
            fi
            
            # Check if sudo.plist exists in the current directory
            if [[ ! -f "sudo.plist" ]]; then
                error_exit "sudo.plist not found in the current directory"
            fi
            
            # Extract OpenSourceVersion (current sudo version)
            currentSudoVersion=$(plutil -extract OpenSourceVersion raw -o - sudo.plist)
            if [[ -z "$currentSudoVersion" ]]; then
                error_exit "Failed to extract OpenSourceVersion from sudo.plist"
            fi
            print_status "Current sudo version: $currentSudoVersion"
            
            # Create working folder
            workingFolder="/var/tmp/sudo-patch-temp"
            mkdir -p "$workingFolder" || error_exit "Failed to create working folder"
            
            # Determine the patch file path
            diffFile="$workingFolder/sudo-patch-$currentSudoVersion.diff"
            
            # Check if the patch file exists
            if [[ ! -f "$diffFile" ]]; then
                print_status "Patch file not found: $diffFile"
                print_status "Creating the patch file first..."
                
                # Extract OpenSourceURL
                sourceURL=$(plutil -extract OpenSourceURL raw -o - sudo.plist)
                if [[ -z "$sourceURL" ]]; then
                    error_exit "Failed to extract OpenSourceURL from sudo.plist"
                fi
                
                # Download the current version
                currentVersionFile="$workingFolder/sudo-$currentSudoVersion.tar.gz"
                print_status "Downloading current sudo version from $sourceURL"
                curl -L "$sourceURL" -o "$currentVersionFile" || error_exit "Failed to download current sudo version"
                
                # Unpack the current version
                vanillaFolder="$workingFolder/sudo-vanilla-$currentSudoVersion"
                print_status "Unpacking current sudo version to $vanillaFolder"
                mkdir -p "$vanillaFolder" || error_exit "Failed to create vanilla folder"
                tar -xzf "$currentVersionFile" -C "$vanillaFolder" --strip-components=1 || error_exit "Failed to unpack current sudo version"
                
                # Create the diff
                create_diff "$vanillaFolder" "$diffFile"
            fi
            
            # Backup current sudo directory
            print_status "Backing up current sudo directory to sudo-backup"
            if [[ -d "sudo-backup" ]]; then
                print_status "Directory sudo-backup already exists, removing it"
                rm -rf "sudo-backup"
            fi
            cp -R "sudo" "sudo-backup" || error_exit "Failed to backup sudo directory"
            
            # Apply patch to the sudo directory
            # Get full path of sudo directory
            local sudoFullPath=$(cd "sudo" && pwd)
            print_status "Attempting to apply patch to sudo directory ($sudoFullPath)"
            if apply_patch "$diffFile" "sudo"; then
                print_status "Patch applied successfully to sudo directory"
            else
                print_status "Patch failed to apply (see details above). Restoring from backup..."
                rm -rf "sudo"
                mv "sudo-backup" "sudo"
                print_status "Restored from backup."
                exit 1
            fi
            
            exit 0
            ;;
        --make-diff)
            print_status "Running in make-diff-only mode"
            
            # Check if sudo directory exists in the current directory
            if [[ ! -d "sudo" ]]; then
                error_exit "sudo directory not found in the current directory"
            fi
            
            # Check if sudo.plist exists in the current directory
            if [[ ! -f "sudo.plist" ]]; then
                error_exit "sudo.plist not found in the current directory"
            fi
            
            # Extract OpenSourceVersion (current sudo version)
            currentSudoVersion=$(plutil -extract OpenSourceVersion raw -o - sudo.plist)
            if [[ -z "$currentSudoVersion" ]]; then
                error_exit "Failed to extract OpenSourceVersion from sudo.plist"
            fi
            print_status "Current sudo version: $currentSudoVersion"
            
            # Create working folder
            workingFolder="/var/tmp/sudo-diff-temp"
            mkdir -p "$workingFolder" || error_exit "Failed to create working folder"
            
            # Extract OpenSourceURL
            sourceURL=$(plutil -extract OpenSourceURL raw -o - sudo.plist)
            if [[ -z "$sourceURL" ]]; then
                error_exit "Failed to extract OpenSourceURL from sudo.plist"
            fi
            
            # Download the current version
            currentVersionFile="$workingFolder/sudo-$currentSudoVersion.tar.gz"
            print_status "Downloading current sudo version from $sourceURL"
            curl -L "$sourceURL" -o "$currentVersionFile" || error_exit "Failed to download current sudo version"
            
            # Unpack the current version
            vanillaFolder="$workingFolder/sudo-vanilla-$currentSudoVersion"
            print_status "Unpacking current sudo version to $vanillaFolder"
            mkdir -p "$vanillaFolder" || error_exit "Failed to create vanilla folder"
            tar -xzf "$currentVersionFile" -C "$vanillaFolder" --strip-components=1 || error_exit "Failed to unpack current sudo version"
            
            # Determine the diff file path
            diffFile="$workingFolder/sudo-patch-$currentSudoVersion.diff"
            
            # Create the diff
            create_diff "$vanillaFolder" "$diffFile"
            
            print_status "Diff file created: $diffFile"
            exit 0
            ;;
        --ignore-sha)
            print_status "SHA checksum verification will be ignored"
            ignore_sha=true
            # If this is the only parameter, show usage
            if [[ $# -eq 1 ]]; then
                error_exit "Usage: $0 <new_sudo_version> --ignore-sha or $0 --ignore-sha --installsrc <new_sudo_version>"
            fi
            # Check if the next parameter is --installsrc
            if [[ "$2" == "--installsrc" ]]; then
                # Shift to the --installsrc parameter
                shift
                # Now handle as if --installsrc was the first parameter
                if [[ $# -ne 2 ]]; then
                    error_exit "Usage: $0 --ignore-sha --installsrc <new_sudo_version>"
                fi
                
                # Get the new sudo version from the second parameter
                newSudoVersion="$2"
                print_status "Installing source for sudo version $newSudoVersion"
                
                # Check if sudo.plist exists in the current directory
                if [[ ! -f "sudo.plist" ]]; then
                    error_exit "sudo.plist not found in the current directory"
                fi
                
                print_status "Reading sudo.plist file"
                # Extract OpenSourceVersion (current sudo version)
                currentSudoVersion=$(plutil -extract OpenSourceVersion raw -o - sudo.plist)
                if [[ -z "$currentSudoVersion" ]]; then
                    error_exit "Failed to extract OpenSourceVersion from sudo.plist"
                fi
                print_status "Current sudo version: $currentSudoVersion"
                
                # Create working folder
                workingFolder="/var/tmp/sudo-installsrc-$newSudoVersion"
                
                # Remove working folder if it exists from previous run
                if [[ -d "$workingFolder" ]]; then
                    print_status "Removing existing working folder from previous run"
                    rm -rf "$workingFolder" || error_exit "Failed to remove existing working folder"
                fi
                
                print_status "Creating working folder: $workingFolder"
                mkdir -p "$workingFolder" || error_exit "Failed to create working folder"
                mkdir -p "$workingFolder/download" || error_exit "Failed to create download folder"
                
                # Extract OpenSourceURL
                sourceURL=$(plutil -extract OpenSourceURL raw -o - sudo.plist)
                if [[ -z "$sourceURL" ]]; then
                    error_exit "Failed to extract OpenSourceURL from sudo.plist"
                fi
                print_status "Source URL: $sourceURL"
                
                # Extract OpenSourceSHA256
                sourceSHA256=$(plutil -extract OpenSourceSHA256 raw -o - sudo.plist)
                if [[ -z "$sourceSHA256" ]]; then
                    error_exit "Failed to extract OpenSourceSHA256 from sudo.plist"
                fi
                print_status "Source SHA256: $sourceSHA256"
                
                # Download the current version
                currentVersionFile="$workingFolder/download/sudo-$currentSudoVersion.tar.gz"
                print_status "Downloading current sudo version from $sourceURL"
                curl -L "$sourceURL" -o "$currentVersionFile" || error_exit "Failed to download current sudo version"
                
                # Verify SHA256 checksum
                print_status "Verifying SHA256 checksum"
                calculatedSHA256=$(shasum -a 256 "$currentVersionFile" | awk '{print $1}')
                if [[ "$calculatedSHA256" != "$sourceSHA256" ]]; then
                    if $ignore_sha; then
                        print_status "SHA256 checksum mismatch ignored. Expected: $sourceSHA256, Got: $calculatedSHA256"
                    else
                        error_exit "SHA256 checksum verification failed. Expected: $sourceSHA256, Got: $calculatedSHA256"
                    fi
                else
                    print_status "SHA256 checksum verified successfully"
                fi
                
                # Unpack the current version
                vanillaFolder="$workingFolder/sudo-vanilla-$currentSudoVersion"
                print_status "Unpacking current sudo version to $vanillaFolder"
                mkdir -p "$vanillaFolder" || error_exit "Failed to create vanilla folder"
                tar -xzf "$currentVersionFile" -C "$vanillaFolder" --strip-components=1 || error_exit "Failed to unpack current sudo version"
                
                # Check if sudo directory exists in the current directory
                if [[ ! -d "sudo" ]]; then
                    error_exit "sudo directory not found in the current directory"
                fi
                
                # Create diff between current sudo and vanilla version
                diffFile="$workingFolder/sudo-patch-$currentSudoVersion.diff"
                create_diff "$vanillaFolder" "$diffFile"
                
                # Create URL for the new version
                newSourceURL="${sourceURL/$currentSudoVersion/$newSudoVersion}"
                print_status "New source URL: $newSourceURL"
                
                # Download the new version
                newVersionFile="$workingFolder/download/sudo-$newSudoVersion.tar.gz"
                print_status "Downloading new sudo version from $newSourceURL"
                curl -L "$newSourceURL" -o "$newVersionFile" || error_exit "Failed to download new sudo version"
                
                # Calculate SHA256 for the new version
                newSourceSHA256=$(shasum -a 256 "$newVersionFile" | awk '{print $1}')
                print_status "New source SHA256: $newSourceSHA256"
                
                # Unpack the new version
                newVanillaFolder="$workingFolder/sudo-vanilla-$newSudoVersion"
                print_status "Unpacking new sudo version to $newVanillaFolder"
                mkdir -p "$newVanillaFolder" || error_exit "Failed to create new vanilla folder"
                tar -xzf "$newVersionFile" -C "$newVanillaFolder" --strip-components=1 || error_exit "Failed to unpack new sudo version"
                
                # Rename current sudo directory to sudo-$currentSudoVersion
                print_status "Renaming current sudo directory to sudo-$currentSudoVersion"
                if [[ -d "sudo-$currentSudoVersion" ]]; then
                    print_status "Directory sudo-$currentSudoVersion already exists, removing it"
                    rm -rf "sudo-$currentSudoVersion"
                fi
                mv "sudo" "sudo-$currentSudoVersion" || error_exit "Failed to rename sudo directory"
                
                # Copy new vanilla folder to current directory as "sudo"
                print_status "Copying new vanilla version ($newVanillaFolder) to current directory as sudo"
                cp -R "$newVanillaFolder" "sudo" || error_exit "Failed to copy new vanilla version"
                
                # Update sudo.plist with new version information
                print_status "Updating sudo.plist with new version information"
                # Get current date in the format YYYY-MM-DD
                currentDate=$(date +"%Y-%m-%d")
                
                # Create a temporary file with the updated values
                tempPlist=$(mktemp)
                plutil -convert xml1 sudo.plist -o "$tempPlist"
                
                # Update OpenSourceVersion
                plutil -replace OpenSourceVersion -string "$newSudoVersion" "$tempPlist"
                
                # Update OpenSourceURL
                plutil -replace OpenSourceURL -string "$newSourceURL" "$tempPlist"
                
                # Update OpenSourceSHA256
                plutil -replace OpenSourceSHA256 -string "$newSourceSHA256" "$tempPlist"
                
                # Update OpenSourceImportDate
                plutil -replace OpenSourceImportDate -string "$currentDate" "$tempPlist"
                
                # Replace the original file
                mv "$tempPlist" sudo.plist
                
                print_status "Installation of source completed successfully"
                print_status "You can now run '$0 --configure' to configure sudo"
                print_status "Then run '$0 --patch' to apply the patch"
                exit 0
            fi
            # Shift to the next parameter
            shift
            ;;
        -h|--help)
            display_help
            ;;
    esac
fi

# Check if we have the required parameter for full update
if [[ $# -ne 1 ]]; then
    error_exit "Usage: $0 <new_sudo_version> or $0 --configure or $0 --patch or $0 --make-diff or $0 --installsrc <new_sudo_version> or $0 --ignore-sha <new_sudo_version> or $0 --help"
fi

# Get the new sudo version from the first parameter
newSudoVersion="$1"
print_status "Starting sudo update to version $newSudoVersion"

# Create working folder
workingFolder="/var/tmp/sudo-update-to-$newSudoVersion"

# Remove working folder if it exists from previous run
if [[ -d "$workingFolder" ]]; then
    print_status "Removing existing working folder from previous run"
    rm -rf "$workingFolder" || error_exit "Failed to remove existing working folder"
fi

print_status "Creating working folder: $workingFolder"
mkdir -p "$workingFolder" || error_exit "Failed to create working folder"
mkdir -p "$workingFolder/download" || error_exit "Failed to create download folder"

# Check if sudo.plist exists in the current directory
if [[ ! -f "sudo.plist" ]]; then
    error_exit "sudo.plist not found in the current directory"
fi

print_status "Reading sudo.plist file"
# Extract OpenSourceVersion (current sudo version)
currentSudoVersion=$(plutil -extract OpenSourceVersion raw -o - sudo.plist)
if [[ -z "$currentSudoVersion" ]]; then
    error_exit "Failed to extract OpenSourceVersion from sudo.plist"
fi
print_status "Current sudo version: $currentSudoVersion"

# Debug function to print script progress
debug_print() {
    echo "DEBUG: $1"
}

# Function to check if version1 is newer than version2
is_newer_version() {
    local v1="$1"
    local v2="$2"
    
    print_status "Comparing versions: $v1 vs $v2"
    
    # If versions are equal, v1 is not newer
    if [[ "$v1" = "$v2" ]]; then
        print_status "Versions are equal"
        return 1  # False, not newer
    fi
    
    # Use sort with -V flag for version comparison
    # If v2 comes first in sorted order, v1 is newer
    if [[ "$(printf '%s\n%s' "$v2" "$v1" | sort -V | head -n1)" = "$v2" ]]; then
        print_status "Version $v1 is newer than $v2"
        return 0  # True, v1 is newer
    else
        print_status "Version $v1 is older than $v2"
        return 1  # False, v1 is not newer
    fi
}

# Function is now defined earlier in the script

# Check if new version is newer than current version
if is_newer_version "$newSudoVersion" "$currentSudoVersion"; then
    print_status "New version ($newSudoVersion) is newer than current version ($currentSudoVersion). Proceeding with update."
    debug_print "Continuing with update process"
else
    print_status "New version ($newSudoVersion) is not newer than current version ($currentSudoVersion). Skipping update."
    configure_sudo
    exit 0
fi

# Extract OpenSourceURL
debug_print "About to extract OpenSourceURL"
sourceURL=$(plutil -extract OpenSourceURL raw -o - sudo.plist)
debug_print "OpenSourceURL extracted: $sourceURL"
if [[ -z "$sourceURL" ]]; then
    error_exit "Failed to extract OpenSourceURL from sudo.plist"
fi
print_status "Source URL: $sourceURL"

# Extract OpenSourceSHA256
sourceSHA256=$(plutil -extract OpenSourceSHA256 raw -o - sudo.plist)
if [[ -z "$sourceSHA256" ]]; then
    error_exit "Failed to extract OpenSourceSHA256 from sudo.plist"
fi
print_status "Source SHA256: $sourceSHA256"

# Download the current version
currentVersionFile="$workingFolder/download/sudo-$currentSudoVersion.tar.gz"
print_status "Downloading current sudo version from $sourceURL"
curl -L "$sourceURL" -o "$currentVersionFile" || error_exit "Failed to download current sudo version"

# Verify SHA256 checksum
print_status "Verifying SHA256 checksum"
calculatedSHA256=$(shasum -a 256 "$currentVersionFile" | awk '{print $1}')
if [[ "$calculatedSHA256" != "$sourceSHA256" ]]; then
    if $ignore_sha; then
        print_status "SHA256 checksum mismatch ignored. Expected: $sourceSHA256, Got: $calculatedSHA256"
    else
        error_exit "SHA256 checksum verification failed. Expected: $sourceSHA256, Got: $calculatedSHA256"
    fi
else
    print_status "SHA256 checksum verified successfully"
fi

# Unpack the current version
vanillaFolder="$workingFolder/sudo-vanilla-$currentSudoVersion"
print_status "Unpacking current sudo version to $vanillaFolder"
mkdir -p "$vanillaFolder" || error_exit "Failed to create vanilla folder"
tar -xzf "$currentVersionFile" -C "$vanillaFolder" --strip-components=1 || error_exit "Failed to unpack current sudo version"

# Check if sudo directory exists in the current directory
if [[ ! -d "sudo" ]]; then
    error_exit "sudo directory not found in the current directory"
fi

# Sync git project in the current directory
print_status "Syncing git project in the current directory"
git -C sudo pull || error_exit "Failed to sync git project"

# Create diff between current sudo and vanilla version
diffFile="$workingFolder/sudo-patch-$currentSudoVersion.diff"
create_diff "$vanillaFolder" "$diffFile"

# Create URL for the new version
newSourceURL="${sourceURL/$currentSudoVersion/$newSudoVersion}"
print_status "New source URL: $newSourceURL"

# Download the new version
newVersionFile="$workingFolder/download/sudo-$newSudoVersion.tar.gz"
print_status "Downloading new sudo version from $newSourceURL"
curl -L "$newSourceURL" -o "$newVersionFile" || error_exit "Failed to download new sudo version"

# Calculate SHA256 for the new version
newSourceSHA256=$(shasum -a 256 "$newVersionFile" | awk '{print $1}')
print_status "New source SHA256: $newSourceSHA256"

# Unpack the new version
newVanillaFolder="$workingFolder/sudo-vanilla-$newSudoVersion"
print_status "Unpacking new sudo version to $newVanillaFolder"
mkdir -p "$newVanillaFolder" || error_exit "Failed to create new vanilla folder"
tar -xzf "$newVersionFile" -C "$newVanillaFolder" --strip-components=1 || error_exit "Failed to unpack new sudo version"

# Copy new vanilla to patched folder
patchedFolder="$workingFolder/sudo-$newSudoVersion"
print_status "Copying new vanilla version to patched folder: $patchedFolder"
cp -R "$newVanillaFolder" "$patchedFolder" || error_exit "Failed to copy new vanilla version"

# Update sudo.plist regardless of patch success
update_plist() {
    print_status "Updating sudo.plist with new version information"
    # Get current date in the format YYYY-MM-DD
    currentDate=$(date +"%Y-%m-%d")

    # Create a temporary file with the updated values
    tempPlist=$(mktemp)
    plutil -convert xml1 sudo.plist -o "$tempPlist"

    # Update OpenSourceVersion
    plutil -replace OpenSourceVersion -string "$newSudoVersion" "$tempPlist"

    # Update OpenSourceURL
    plutil -replace OpenSourceURL -string "$newSourceURL" "$tempPlist"

    # Update OpenSourceSHA256
    plutil -replace OpenSourceSHA256 -string "$newSourceSHA256" "$tempPlist"

    # Update OpenSourceImportDate
    plutil -replace OpenSourceImportDate -string "$currentDate" "$tempPlist"

    # Replace the original file
    mv "$tempPlist" sudo.plist
    
    print_status "sudo.plist has been updated with new version information"
}

# Rename current sudo directory to sudo-$currentSudoVersion
print_status "Renaming current sudo directory to sudo-$currentSudoVersion"
if [[ -d "sudo-$currentSudoVersion" ]]; then
    print_status "Directory sudo-$currentSudoVersion already exists, removing it"
    rm -rf "sudo-$currentSudoVersion"
fi
mv "sudo" "sudo-$currentSudoVersion" || error_exit "Failed to rename sudo directory"

# Copy new vanilla folder to current directory as "sudo"
print_status "Copying new vanilla version ($newVanillaFolder) to current directory as sudo"
cp -R "$newVanillaFolder" "sudo" || error_exit "Failed to copy new vanilla version"

# Configure sudo first
print_status "Configuring new sudo version before patching"
configure_sudo

# Now apply the patch to the configured sudo directory
print_status "Applying patch to configured sudo directory"
patchSuccess=false

# Apply patch to the sudo directory
# Get full path of sudo directory
local sudoFullPath=$(cd "sudo" && pwd)
print_status "Attempting to apply patch to sudo directory ($sudoFullPath)"
if apply_patch "$diffFile" "sudo"; then
    patchSuccess=true
    print_status "Successfully updated sudo to version $newSudoVersion"
else
    print_status "Patch failed to apply (see details above). sudo.plist will still be updated, but patch changes were not applied."
fi

# Update sudo.plist regardless of patch success
update_plist

if $patchSuccess; then
    print_status "Updated and patched files are in the sudo directory"
else
    print_status "Updated files are in the sudo directory, but patch failed to apply"
fi

print_status "Update process completed successfully"
exit 0