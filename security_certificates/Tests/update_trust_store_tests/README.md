# Trust Store Update Testing Suite

A comprehensive testing framework for `update_trust_store.py` that provides automated testing, OID validation, and interactive CLI demonstrations for certificate trust store management.

## Overview

This testing suite validates all aspects of trust store updates including:
- Certificate addition, removal, and modification operations
- Policy constraint and EV TLS OID validation
- Version management and file system operations
- Command-line interface functionality
- Error handling and edge cases

## Quick Start

### Run All Tests
```bash
# Complete automated test suite
python3 test_update_trust_store.py

# Verbose output with detailed results
python3 test_update_trust_store.py -v
```

### Run Specific Test Categories
```bash
# Core trust store operations
python3 -m unittest test_update_trust_store.TrustStoreUpdateTests -v

# OID validation functionality
python3 -m unittest test_update_trust_store.OIDValidationTests -v

# CLI interface testing
python3 -m unittest test_update_trust_store.CLITests -v

# End-to-end OID integration
python3 -m unittest test_update_trust_store.OIDIntegrationTests -v
```

### Interactive CLI Demonstration
```bash
# Make script executable and run interactive examples
chmod +x run_cli_examples.sh
./run_cli_examples.sh
```

### Standalone OID Validation
```bash
# Test OID validation functions directly
python3 oid_validation.py
```

### Pinning Parity Validation
```bash
# Validate pinning rule parity (see DEFAULT_COMPARISONS in test_pinning_parity.py)
python3 test_pinning_parity.py

# Compare two arbitrary suffixes (full parity required)
python3 test_pinning_parity.py --suffix-a example.com --suffix-b example.co.uk
```

## Test Data Files

The suite includes comprehensive test data based on real certificate examples:

### Core Test Files
- **test_data_addition.json** - Tests certificate addition with Apple policy constraints
- **test_data_removal.json** - Tests certificate removal from system anchors
- **test_data_modification.json** - Tests certificate modification with EV OIDs
- **test_data_mixed.json** - Tests batch operations with multiple certificates
- **test_data_v1_schema.json** - Tests backward compatibility with v1 schema
- **test_data_bad_cer.json** - Tests edge cases with bad certificate

### Certificate Examples Used
- **GTS Root R1** (Google Trust Services) - Addition testing
- **Cisco Root CA 2048** - Removal testing  
- **GTS Root R4** - Modification with EV OIDs
- **DigiCert TLS RSA4096 Root G5** - System anchor with EV support
- **Apple RCS Signing ECC Root CA** - Custom anchor with policy constraints

## Test Coverage

### Operations Tested
| Operation | Test Cases | Dry Run | Actual Execution | Error Handling |
|-----------|------------|---------|------------------|----------------|
| **Addition** | ✅ | ✅ | ✅ | ✅ |
| **Removal** | ✅ | ✅ | ✅ | ✅ |
| **Modification** | ✅ | ✅ | ✅ | ✅ |
| **Mixed Batch** | ✅ | ✅ | ✅ | ✅ |

### Anchor Types Supported
- **System** - Built-in system trust anchors
- **Custom** - User-added trust anchors
- **Platform** - Platform-specific trust anchors
- **Test-System** - System anchors for testing
- **Test-Platform** - Platform anchors for testing

### Features Validated
- **Policy Constraints** - Apple policy OID validation (1.2.840.113635.100.1.*)
- **EV TLS OIDs** - Extended Validation certificate validation
- **Version Management** - Automatic version updates and overrides
- **File Operations** - Certificate files, JSON configs, plist updates
- **CLI Interface** - All command-line options and error handling

## Usage Examples

### Basic Operations
```bash
# Test certificate addition with dry run
python3 update_trust_store.py \
    --update_json test_data_addition.json \
    --srcroot /tmp/test_trust_store \
    --dry_run

# Test with version override
python3 update_trust_store.py \
    --update_json test_data_mixed.json \
    --srcroot /tmp/test_trust_store \
    --dry_run \
    --asset_version "2.5.0"

# Test skipping version updates
python3 update_trust_store.py \
    --update_json test_data_modification.json \
    --srcroot /tmp/test_trust_store \
    --dry_run \
    --no_version_update
```

### Advanced Testing
```bash
# Test specific test method
python3 -m unittest test_update_trust_store.TrustStoreUpdateTests.test_actual_addition -v

# Test OID validation integration
python3 -m unittest test_update_trust_store.OIDIntegrationTests.test_valid_oids_file -v

# Test CLI argument parsing
python3 -m unittest test_update_trust_store.CLITests.test_help_output -v
```

### OID Validation Examples
```bash
# Validate OIDs in existing test data
python3 -c "
from oid_validation import validate_update_json_oids
import json

# Load and validate test data
with open('test_data_addition.json', 'r') as f:
    data = json.load(f)
    
result = validate_update_json_oids(data)
print(f'Validation result: {result[\"valid\"]}')

# Show any errors
for error in result['errors']:
    print(f'Error: {error}')

# Show certificate-specific results
for cert_result in result['certificate_results']:
    print(f'Certificate: {cert_result[\"certificate\"]}')
    print(f'  Valid policy constraints: {cert_result[\"policy_constraints\"][\"valid\"]}')
    print(f'  Valid EV OIDs: {cert_result[\"ev_oids\"][\"valid\"]}')
"
```

## Test Architecture

### Test Suite Structure
```
test_update_trust_store.py
├── TrustStoreUpdateTests     # Core functionality testing
├── CLITests                  # Command-line interface validation  
├── OIDValidationTests        # OID format and policy validation
└── OIDIntegrationTests       # End-to-end OID workflows
```

### Supporting Files
```
oid_validation.py             # OID validation utilities
run_cli_examples.sh          # Interactive CLI demonstrations
test_pinning_parity.py       # Pinning rule parity validation
test_data_*.json             # Comprehensive test data files
TEST_DOCUMENTATION.md        # Detailed testing documentation
```

## File System Testing

The test suite validates all file operations:

### Certificate Files
- **Creation**: Writing .cer files to appropriate directories
- **Naming**: SHA256 fingerprint-based naming convention
- **Content**: PEM format certificate validation

### JSON Configuration Files
- **constraints.json**: Policy constraint updates per anchor type
- **EVRoots.json**: EV TLS OID configuration updates
- **hash_to_human_name.json**: Human-readable certificate name mapping

### Version Management Files
- **AssetVersion.plist**: Binary plist with version numbers
- **Info-Asset.plist**: Bundle information and version strings
- **security_certificates.xcconfig**: Build configuration version

## Error Handling Testing

The suite includes comprehensive error testing:

### Input Validation
- Invalid JSON file paths
- Malformed JSON structure
- Missing required fields
- Invalid anchor types
- Malformed OIDs

### File System Errors
- Missing source root directory
- Permission issues (simulated)
- Disk space issues (simulated)
- Concurrent access scenarios

### OID Validation Errors
- Invalid OID format (non-numeric, consecutive dots)
- Non-Apple policy OIDs in policy constraints
- Invalid EV TLS OIDs
- Empty or null OID values

## Safety Features

### Isolated Testing Environment
- All tests run in temporary directories (`/tmp/trust_store_test_*`)
- No modification of actual trust store files
- Automatic cleanup after test completion
- Mock directory structure creation

### Dry Run Validation
- Complete operation simulation without file changes
- Output validation for expected messages
- Version calculation verification
- Error condition testing without side effects

## Integration with CI/CD

The test suite is designed for automated environments:

### Exit Codes
- **0**: All tests passed
- **Non-zero**: Test failures detected

### Output Formats
- **Standard**: Basic pass/fail information
- **Verbose (-v)**: Detailed test method results
- **JSON**: Machine-readable results (via unittest)

### Performance Considerations
- **Fast execution**: Most tests complete in under 30 seconds
- **Parallel safe**: Tests use isolated temporary directories
- **Memory efficient**: Cleanup after each test method

## Development Notes

### Adding New Tests
1. Create test data files in JSON format following existing patterns
2. Add test methods to appropriate test classes
3. Update documentation and coverage information
4. Add CLI examples to `run_cli_examples.sh`

### Extending OID Validation
1. Add validation rules to `oid_validation.py`
2. Create corresponding test cases in `OIDValidationTests`
3. Add integration tests in `OIDIntegrationTests`
4. Update documentation with new validation rules

### Debugging Test Failures
```bash
# Run with maximum verbosity
python3 test_update_trust_store.py -v

# Run specific failing test
python3 -m unittest test_update_trust_store.TrustStoreUpdateTests.test_name -v

# Debug with temporary directory inspection
# (Modify tearDown() to skip cleanup temporarily)
```

## Requirements

### Python Dependencies
- **Python 3.6+**: Core language support
- **unittest**: Built-in testing framework
- **tempfile**: Temporary directory creation
- **subprocess**: CLI testing
- **json**: JSON file handling
- **plistlib**: Binary plist file handling

### System Dependencies
- **Bash**: For run_cli_examples.sh execution
- **Write access**: To /tmp directory for test files

## Contributing

When contributing to the test suite:

1. **Test Coverage**: Ensure new functionality includes comprehensive tests
2. **Documentation**: Update both inline comments and markdown documentation
3. **Safety**: Maintain isolated testing environment principles
4. **Compatibility**: Test with multiple Python versions if possible

The test suite serves as both validation and documentation for the trust store update functionality, providing confidence in the reliability and correctness of certificate management operations.
