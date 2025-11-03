#!/bin/bash

# Trust Store Update CLI Examples and Interactive Testing
# This script demonstrates various ways to use the trust store update script
# and provides interactive examples with real-time feedback.

set -e  # Exit on any error

# Color definitions for better output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
BASE_TEST_DIR="/tmp/trust_store_cli_test"
TEST_DIR="${BASE_TEST_DIR}_$(date +%s)"
CURRENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT="${CURRENT_DIR}/../../update_scripts/update_trust_store.py"
OID_SCRIPT="${CURRENT_DIR}/../../update_scripts/oid_validation.py"
TEST_SCRIPT="${CURRENT_DIR}/test_update_trust_store.py"

# Print functions
print_header() {
    echo -e "\n${CYAN}================================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}================================================${NC}\n"
}

print_section() {
    echo -e "\n${BLUE}$1${NC}"
    echo -e "${BLUE}$(printf '%.0s-' {1..50})${NC}\n"
}

print_command() {
    echo -e "${YELLOW}Command:${NC}"
    echo -e "${MAGENTA}$1${NC}\n"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_info() {
    echo -e "${CYAN}ℹ️  $1${NC}"
}

# Setup functions
setup_test_environment() {
    print_section "Setting up test environment"
    
    print_info "Creating test directory: $TEST_DIR"
    mkdir -p "$TEST_DIR"/{certificates/{custom,platform,roots,test-roots,test-platform,removed/intermediates},config,update_scripts}
    
    print_info "Creating required configuration files..."
    
    # Create constraints.json
    cat > "$TEST_DIR/certificates/constraints.json" << 'EOF'
{
    "system": {},
    "custom": {},
    "platform": {},
    "test-system": {},
    "test-platform": {}
}
EOF
    
    # Create EVRoots.json
    cat > "$TEST_DIR/certificates/EVRoots.json" << 'EOF'
{
    "EV_config": {},
    "fingerprint_map": {}
}
EOF
    
    # Create hash_to_human_name.json
    cat > "$TEST_DIR/certificates/hash_to_human_name.json" << 'EOF'
{}
EOF
    
    # Create plist files using Python
    python3 << EOF
import plistlib
import os

# AssetVersion.plist
asset_version = {
    'VersionNumber': 2024082500,
    'PKITrustStoreAssetsVersion': '1.0.0',
    'MobileAssetContentVersion': 123
}
with open('$TEST_DIR/config/AssetVersion.plist', 'wb') as f:
    plistlib.dump(asset_version, f)

# Info-Asset.plist
info_asset = {
    'CFBundleShortVersionString': '1.0.0',
    'CFBundleVersion': '1.0.0',
    'MobileAssetProperties': {
        'AssetVersion': '1.0.0',
        'ContentVersion': 2024082500
    }
}
with open('$TEST_DIR/config/Info-Asset.plist', 'wb') as f:
    plistlib.dump(info_asset, f)
EOF
    
    # Create xcconfig file
    cat > "$TEST_DIR/config/security_certificates.xcconfig" << 'EOF'
// Trust Store Configuration
TRUST_STORE_VERSION = 2024082500
OTHER_SETTING = example_value
BUILD_NUMBER = 1.0.0
EOF
    
    # Copy schema file if it exists
    if [ -f "../update_scripts/trust_store_updates_schema_v2.json" ]; then
        cp "../update_scripts/trust_store_updates_schema_v2.json" "$TEST_DIR/update_scripts/"
        print_info "Schema file copied successfully"
    else
        print_info "Schema file not found - creating minimal schema"
        cat > "$TEST_DIR/update_scripts/trust_store_updates_schema_v2.json" << 'EOF'
{
    "id": "com.apple.trust_store_updates_schema_v2",
    "$schema": "http://json-schema.org/draft-07/schema",
    "type": "array",
    "items": {
        "type": "object",
        "properties": {
            "change_type": {"type": "string", "enum": ["Removal", "Addition", "Modification"]},
            "change_reason": {"type": "string"},
            "certificate_details": {"type": "object"},
            "anchor_type": {"type": "string"}
        },
        "required": ["change_type", "certificate_details", "anchor_type"]
    }
}
EOF
    fi
    
    print_success "Test environment created successfully"
    print_info "Directory structure:"
    find "$TEST_DIR" -type f | head -20 | sed 's/^/  /'
    if [ $(find "$TEST_DIR" -type f | wc -l) -gt 20 ]; then
        echo "  ... and $(( $(find "$TEST_DIR" -type f | wc -l) - 20 )) more files"
    fi
}

check_test_data_files() {
    print_section "Checking test data files"
    
    local test_files=(
        "test_data_addition.json"
        "test_data_removal.json"
        "test_data_modification.json"
        "test_data_mixed.json"
        "test_data_bad_cert.json"
    )
    
    local missing_files=()
    
    for file in "${test_files[@]}"; do
        if [ -f "$CURRENT_DIR/$file" ]; then
            print_success "Found: $file"
        else
            print_error "Missing: $file"
            missing_files+=("$file")
        fi
    done
    
    if [ ${#missing_files[@]} -gt 0 ]; then
        print_info "Some test data files are missing. CLI examples will skip those tests."
        print_info "Missing files: ${missing_files[*]}"
    fi
}

check_script_availability() {
    print_section "Checking script availability"
    
    if [ -f "$SCRIPT" ]; then
        print_success "Found update_trust_store.py script"
        # Check if script is executable with Python
        if python3 "$SCRIPT" --help > /dev/null 2>&1; then
            print_success "Script is executable and working"
        else
            print_error "Script found but not working properly"
            return 1
        fi
    else
        print_error "update_trust_store.py script not found at $SCRIPT"
        return 1
    fi
}

run_example() {
    local title="$1"
    local description="$2"
    local cmd="$3"
    local test_file="$4"
    
    print_section "$title"
    
    if [ -n "$description" ]; then
        print_info "$description"
    fi
    
    # Check if test file exists (if specified)
    if [ -n "$test_file" ] && [ ! -f "$test_file" ]; then
        print_error "Test file $test_file not found - skipping this example"
        return 0
    fi
    
    print_command "$cmd"
    
    print_info "Running command..."
    echo "----------------------------------------"
    
    # Execute the command and capture both stdout and stderr
    if eval "$cmd"; then
        echo "----------------------------------------"
        print_success "Command completed successfully"
    else
        local exit_code=$?
        echo "----------------------------------------"
        print_error "Command failed with exit code $exit_code"
        if [ "$exit_code" -ne 0 ] && [ -n "$test_file" ]; then
            print_info "This might be expected for demonstration purposes"
        fi
        return $exit_code
    fi
}

run_oid_validation_examples() {
    print_section "OID Validation Examples"
    
    if [ -f "${OID_SCRIPT}" ]; then
        print_info "Running standalone OID validation tests"
        print_command "python3 oid_validation.py"
        python3 ${OID_SCRIPT}
        print_success "OID validation completed"
    else
        print_info "oid_validation.py not found - skipping OID validation examples"
    fi
}

run_unit_tests() {
    print_section "Running Unit Tests"
    
    if [ -f "${TEST_SCRIPT}" ]; then
        print_info "Running comprehensive test suite"
        print_command "python3 test_update_trust_store.py"
        
        echo "Running tests with summary output..."
        if python3 ${TEST_SCRIPT} 2>&1 | tee /tmp/test_output.log; then
            print_success "All tests passed"
            
            # Show test summary
            echo ""
            print_info "Test Summary:"
            grep -E "(test_|OK|FAILED|ERROR)" /tmp/test_output.log | tail -10 || true
        else
            print_error "Some tests failed"
            print_info "Check /tmp/test_output.log for details"
        fi
    else
        print_info "test_update_trust_store.py not found - skipping unit tests"
    fi
}

show_file_changes() {
    print_section "Analyzing File Changes"
    
    print_info "Checking what files were modified during actual execution..."
    
    # Check for new certificate files
    local cert_files=$(find "$TEST_DIR/certificates" -name "*.cer" 2>/dev/null | wc -l)
    if [ "$cert_files" -gt 0 ]; then
        print_success "$cert_files certificate file(s) created:"
        find "$TEST_DIR/certificates" -name "*.cer" | sed 's/^/  /' || true
    else
        print_info "No certificate files created"
    fi
    
    # Check configuration file changes
    print_info "Configuration file status:"
    
    # Check AssetVersion.plist
    python3 << EOF
try:
    import plistlib
    with open('$TEST_DIR/config/AssetVersion.plist', 'rb') as f:
        data = plistlib.load(f)
    print(f"  📄 AssetVersion.plist:")
    print(f"    VersionNumber: {data.get('VersionNumber', 'N/A')}")
    print(f"    PKITrustStoreAssetsVersion: {data.get('PKITrustStoreAssetsVersion', 'N/A')}")
    print(f"    MobileAssetContentVersion: {data.get('MobileAssetContentVersion', 'N/A')}")
except Exception as e:
    print(f"  ❌ Error reading AssetVersion.plist: {e}")
EOF
    
    # Check Info-Asset.plist
    python3 << EOF
try:
    import plistlib
    with open('$TEST_DIR/config/Info-Asset.plist', 'rb') as f:
        data = plistlib.load(f)
    print(f"  📄 Info-Asset.plist:")
    print(f"    CFBundleShortVersionString: {data.get('CFBundleShortVersionString', 'N/A')}")
    print(f"    CFBundleVersion: {data.get('CFBundleVersion', 'N/A')}")
    props = data.get('MobileAssetProperties', {})
    print(f"    AssetVersion: {props.get('AssetVersion', 'N/A')}")
    print(f"    ContentVersion: {props.get('ContentVersion', 'N/A')}")
except Exception as e:
    print(f"  ❌ Error reading Info-Asset.plist: {e}")
EOF
    
    # Check xcconfig file
    if [ -f "$TEST_DIR/config/security_certificates.xcconfig" ]; then
        print_info "  📄 security_certificates.xcconfig:"
        grep "TRUST_STORE_VERSION" "$TEST_DIR/config/security_certificates.xcconfig" | sed 's/^/    /' || print_info "    No TRUST_STORE_VERSION found"
    fi
    
    # Check JSON files for changes
    local json_files=("constraints.json" "EVRoots.json" "hash_to_human_name.json")
    for json_file in "${json_files[@]}"; do
        if [ -f "$TEST_DIR/certificates/$json_file" ]; then
            local size=$(stat -f%z "$TEST_DIR/certificates/$json_file" 2>/dev/null || stat -c%s "$TEST_DIR/certificates/$json_file" 2>/dev/null || echo "unknown")
            print_info "  📄 $json_file: $size bytes"
        fi
    done
}

cleanup() {
    print_section "Cleanup"
    
    print_info "Test directory location: $TEST_DIR"
    print_info "Files created during testing:"
    find "$TEST_DIR" | wc -l | xargs printf "  Total files: %s\n"
    
    echo ""
    read -p "Do you want to remove the test directory? [y/N]: " -r
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$TEST_DIR"
        print_success "Test directory removed"
    else
        print_info "Test directory preserved for inspection: $TEST_DIR"
        print_info "You can remove it manually with: rm -rf $TEST_DIR"
    fi
}

main() {
    print_header "Trust Store Update CLI Examples and Testing"
    
    print_info "This script will demonstrate various uses of update_trust_store.py"
    print_info "All operations will be performed in an isolated test environment"
    
    # Setup and checks
    setup_test_environment
    check_script_availability || exit 1
    check_test_data_files
    
    # Basic CLI examples
    run_example \
        "1. Basic Dry Run Test" \
        "Testing certificate addition with dry run mode" \
        "python3 $SCRIPT --update_json $CURRENT_DIR/test_data_addition.json --srcroot $TEST_DIR --dry_run" \
        "$CURRENT_DIR/test_data_addition.json"

    run_example \
        "2. Version Override Test" \
        "Testing version override functionality" \
        "python3 $SCRIPT --update_json $CURRENT_DIR/test_data_mixed.json --srcroot $TEST_DIR --dry_run --asset_version \"2.5.0\"" \
        "$CURRENT_DIR/test_data_mixed.json"

    run_example \
        "3. Skip Version Updates" \
        "Testing with version updates disabled" \
        "python3 $SCRIPT --update_json $CURRENT_DIR/test_data_modification.json --srcroot $TEST_DIR --dry_run --no_version_update" \
        "$CURRENT_DIR/test_data_modification.json"

    run_example \
        "4. Actual Execution" \
        "Performing real trust store update (in test environment)" \
        "python3 $SCRIPT --update_json $CURRENT_DIR/test_data_addition.json --srcroot $TEST_DIR" \
        "$CURRENT_DIR/test_data_addition.json"

    run_example \
        "5. Help Output" \
        "Displaying help information" \
        "python3 $SCRIPT --help"
    
    # Error handling examples
    print_section "Error Handling Examples"
    
    print_info "Testing various error conditions..."
    
    run_example \
        "Missing Required Argument" \
        "Testing behavior with missing --update_json" \
        "python3 $SCRIPT --srcroot $TEST_DIR 2>&1 || true"
    
    run_example \
        "Invalid JSON File" \
        "Testing with non-existent JSON file" \
        "python3 $SCRIPT --update_json /nonexistent.json --srcroot $TEST_DIR --dry_run 2>&1 || true"
    
    run_example \
        "Invalid Source Root" \
        "Testing with non-existent source root" \
        "python3 $SCRIPT --update_json $CURRENT_DIR/test_data_addition.json --srcroot /nonexistent --dry_run 2>&1 || true" \
        "$CURRENT_DIR/test_data_addition.json"
    
    # OID validation examples
    run_oid_validation_examples
    
    # Unit tests
    run_unit_tests
    
    # Show results
    show_file_changes
    
    # Summary
    print_header "Test Results Summary"
    print_success "CLI examples completed successfully"
    print_info "Test environment: $TEST_DIR"
    print_info "All operations were performed safely in isolation"
    
    # Cleanup
    cleanup
    
    print_header "CLI Examples Complete"
    print_success "All demonstrations completed"
    print_info "Thank you for using the Trust Store Update CLI testing suite!"
}

# Handle script interruption
trap 'echo -e "\n${RED}Script interrupted${NC}"; cleanup; exit 1' INT TERM

# Run main function
main "$@"
