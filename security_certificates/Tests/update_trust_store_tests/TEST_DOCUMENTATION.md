# Trust Store Update Testing Suite
This directory contains comprehensive test data and test scripts for `update_trust_store.py`, including OID validation and certificate processing functionality.

## Quick Start
```bash
# Run the complete test suite
python3 test_update_trust_store.py

# Run specific test categories
python3 -m unittest test_update_trust_store.TrustStoreUpdateTests -v
python3 -m unittest test_update_trust_store.OIDValidationTests -v
python3 -m unittest test_update_trust_store.OIDIntegrationTests -v

# Run interactive CLI examples 
chmod +x run_cli_examples.sh
./run_cli_examples.sh

# Quick OID validation test
python3 oid_validation.py

# Pinning parity test (see DEFAULT_COMPARISONS in test_pinning_parity.py)
python3 test_pinning_parity.py
```

## Test Architecture

### Core Test Files
1. **test_update_trust_store.py** - Unified comprehensive test suite
   - **TrustStoreUpdateTests**: Core functionality testing
   - **CLITests**: Command-line interface validation
   - **OIDValidationTests**: OID format and policy validation
   - **OIDIntegrationTests**: End-to-end OID validation workflows

2. **oid_validation.py** - OID validation utilities and standalone testing
   - Apple policy OID validation (1.2.840.113635.100.1.*)
   - EV TLS OID validation (multiple CA arcs)
   - JSON update file OID validation
   - Standalone validation functions

3. **run_cli_examples.sh** - Interactive demonstration script
   - Live CLI usage examples
   - Error handling demonstrations
   - Real-time test environment setup

4. **test_pinning_parity.py** - Pinning rule parity validation
   - Compares `labelRegex` alternatives between paired domain suffixes in `CertificatePinning.plist`
   - Default comparison pairs are defined in `DEFAULT_COMPARISONS` near the top of the file
   - General-purpose CLI for comparing any two suffixes

### Test Data Files (JSON)
Generated test files based on real certificate examples:

1. **test_data_addition.json** - Certificate addition with policy constraints
   - **Certificate**: GTS Root R1
   - **Anchor Type**: Custom
   - **Policy Constraints**: 1.2.840.113635.100.1.123 (Apple iOS App Store)

2. **test_data_removal.json** - Certificate removal operations
   - **Certificate**: Cisco Root CA 2048
   - **Anchor Type**: System
   - **Operation**: Clean removal from trust store

3. **test_data_modification.json** - Certificate modification with EV OIDs
   - **Certificate**: GTS Root R4
   - **Anchor Type**: Platform
   - **Features**: Policy constraints + EV TLS OIDs (2.23.140.1.1)

4. **test_data_mixed.json** - Multi-certificate operations
   - **DigiCert TLS RSA4096 Root G5**: System anchor with EV OIDs
   - **Apple RCS Signing ECC Root CA**: Custom anchor with policy constraints
   - **Test Apple Platform Bootstrap ECC Root CA**: Test-System with mixed constraints

5. **test_data_v1_schema.json** - Legacy v1 schema compatibility
   - Based on 127443468.json structure
   - Includes "valid" section for backward compatibility

6. **test_data_bad_cer.json** - Minimal test case for bad certificate input

## Test Coverage Matrix

### Operations Coverage
| Operation | Dry Run | Actual | Version Mgmt | Error Handling |
|-----------|---------|--------|--------------|----------------|
| Addition | ✅ | ✅ | ✅ | ✅ |
| Removal | ✅ | ✅ | ✅ | ✅ |
| Modification | ✅ | ✅ | ✅ | ✅ |
| Mixed Batch | ✅ | ✅ | ✅ | ✅ |

### Anchor Types Coverage
| Anchor Type | Addition | Removal | Modification | Policy Constraints | EV OIDs |
|-------------|----------|---------|--------------|-------------------|---------|
| System | ✅ | ✅ | ✅ | ✅ | ✅ |
| Custom | ✅ | ✅ | ✅ | ✅ | ✅ |
| Platform | ✅ | ✅ | ✅ | ✅ | ✅ |
| Test-System | ✅ | ✅ | ✅ | ✅ | ✅ |
| Test-Platform | ✅ | ✅ | ✅ | ✅ | ✅ |

### OID Validation Coverage
| OID Type | Format Validation | Apple Policy | EV TLS | Integration |
|----------|-------------------|--------------|--------|-------------|
| Apple Policy OIDs | ✅ | ✅ | N/A | ✅ |
| EV TLS OIDs | ✅ | N/A | ✅ | ✅ |
| Invalid Formats | ✅ | ✅ | ✅ | ✅ |
| Edge Cases | ✅ | ✅ | ✅ | ✅ |

### CLI Features Coverage
| Feature | Implementation | Testing | Documentation |
|---------|----------------|---------|---------------|
| --dry_run | ✅ | ✅ | ✅ |
| --asset_version | ✅ | ✅ | ✅ |
| --no_version_update | ✅ | ✅ | ✅ |
| --help | ✅ | ✅ | ✅ |
| Error messages | ✅ | ✅ | ✅ |

### File System Coverage
| Component | Read | Write | Update | Validation |
|-----------|------|-------|--------|------------|
| Certificate files (.cer) | ✅ | ✅ | ✅ | ✅ |
| constraints.json | ✅ | ✅ | ✅ | ✅ |
| EVRoots.json | ✅ | ✅ | ✅ | ✅ |
| hash_to_human_name.json | ✅ | ✅ | ✅ | ✅ |
| AssetVersion.plist | ✅ | ✅ | ✅ | ✅ |
| Info-Asset.plist | ✅ | ✅ | ✅ | ✅ |
| security_certificates.xcconfig | ✅ | ✅ | ✅ | ✅ |

## Usage Examples

### Basic Testing
```bash
# Complete test suite with verbose output
python3 test_update_trust_store.py -v

# Test specific functionality
python3 -m unittest test_update_trust_store.TrustStoreUpdateTests.test_dry_run_addition -v
```

### OID Validation Testing
```bash
# Standalone OID validation
python3 oid_validation.py

# OID validation with specific test data
python3 -c "
from oid_validation import validate_update_json_oids
import json
with open('test_data_addition.json', 'r') as f:
    data = json.load(f)
result = validate_update_json_oids(data)
print('Valid:', result['valid'])
for error in result['errors']:
    print('Error:', error)
"
```

### Manual CLI Testing
```bash
# Basic dry run
python3 update_trust_store.py \
    --update_json test_data_addition.json \
    --srcroot /tmp/test_trust_store \
    --dry_run

# Version override with validation
python3 update_trust_store.py \
    --update_json test_data_mixed.json \
    --srcroot /tmp/test_trust_store \
    --dry_run \
    --asset_version "2.5.0"

# Skip version updates for testing
python3 update_trust_store.py \
    --update_json test_data_modification.json \
    --srcroot /tmp/test_trust_store \
    --dry_run \
    --no_version_update
```

### Error Testing
```bash
# Test invalid JSON
python3 update_trust_store.py \
    --update_json /nonexistent.json \
    --srcroot /tmp/test \
    --dry_run

# Test invalid OIDs (if validation is integrated)
python3 -c "
import json
invalid_data = [{
    'change_type': 'Addition',
    'certificate_details': {'sha256_fingerprint': 'ABC123', 'spki': 'test', 'pem': 'test'},
    'anchor_type': 'Custom',
    'policy_constraints': ['invalid.oid.format']
}]
with open('/tmp/invalid_oids.json', 'w') as f:
    json.dump(invalid_data, f)
"
python3 update_trust_store.py --update_json /tmp/invalid_oids.json --srcroot /tmp/test --dry_run
```

## Integration Points

### OID Validation Integration
The test suite includes comprehensive OID validation that can be integrated into the main update process:

1. **Pre-validation**: Validate all OIDs before making any changes
2. **Policy Constraints**: Ensure Apple policy OIDs follow 1.2.840.113635.100.1.* format
3. **EV TLS OIDs**: Validate against known EV CA arcs (CA/Browser Forum, Enterprise, National)
4. **Error Reporting**: Detailed error messages with certificate names and invalid OIDs

### Schema Validation Integration
- JSON Schema validation for update file structure
- Backward compatibility with v1 schema format
- Required field validation
- Type checking for all fields

### Version Management Integration
- Automatic version number generation based on timestamp
- Asset version override capabilities
- Version update skipping for testing scenarios
- Multi-file version synchronization

## Safety Features

### Isolated Testing Environment
- All tests run in temporary directories
- No modification of actual trust store files during testing
- Automatic cleanup after test completion
- Mock file system structure creation

### Dry Run Validation
- Complete operation simulation without file changes
- Output validation for all operations
- Version calculation without actual updates
- Error condition testing without side effects

### Error Handling Coverage
- Invalid file path handling
- Malformed JSON detection
- Missing required field validation
- OID format validation
- Certificate parsing error handling

## Pinning Parity Testing

### Overview
`test_pinning_parity.py` validates that paired domain suffixes in `CertificatePinning.plist`
have consistent `labelRegex` rules. It catches regressions where a regex cleanup on one suffix
silently drops coverage for the paired suffix (see rdar://173515996).

### How It Works
For each policy that contains domain entries for both suffixes, the test splits each
`labelRegex` on `|` into individual alternatives and checks that both sides have the same
set of terms (minus an explicit allowlist for intentional differences).

### Usage
```bash
# Run default comparisons (see DEFAULT_COMPARISONS in test_pinning_parity.py)
python3 test_pinning_parity.py

# Compare two arbitrary suffixes (full parity required, no allowlists)
python3 test_pinning_parity.py --suffix-a example.com --suffix-b example.co.uk

# Test against a different plist
python3 test_pinning_parity.py --plist /path/to/CertificatePinning.plist
```

Per-policy allowlists for known intentional differences are only available
through `DEFAULT_COMPARISONS`; the CLI mode always requires full parity.

### Adding New Default Comparison Pairs
Edit the `DEFAULT_COMPARISONS` list near the top of `test_pinning_parity.py`.
The list format and instructions for adding new entries are documented inline.

## Notes for Developers

### Extending the Test Suite
1. Add new test data files in JSON format
2. Create corresponding test methods in appropriate test classes
3. Update coverage documentation
4. Add CLI examples to run_cli_examples.sh

### OID Validation Extension
1. Add new OID validation rules to oid_validation.py
2. Update test cases in OIDValidationTests
3. Add integration tests in OIDIntegrationTests
4. Document new validation rules

### CI/CD Integration
The test suite is designed for automated testing environments:
- Returns proper exit codes for success/failure
- Provides verbose output for debugging
- Creates isolated test environments
- Cleans up after execution

All tests create isolated temporary environments, ensuring no actual trust store files are ever modified during testing.
