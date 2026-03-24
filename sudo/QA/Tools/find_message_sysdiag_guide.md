# Sysdiagnose Logarchive Analyzer

## Description
This script analyzes macOS sysdiagnose archives to search for specific log messages in the system_logs.logarchive.

## Usage

```bash
./find_message_sysdiag.sh <path_to_sysdiagnose.tar.gz> '<search_string>'
```

### Arguments
- `<path_to_sysdiagnose.tar.gz>` - Path to the sysdiagnose tar.gz file
- `<search_string>` - The message text to search for (use quotes if it contains spaces)

## Example

### Search for "Reading managed config"
```bash
./find_message_sysdiag.sh ~/Downloads/sysdiagnose_2025.10.15_07-04-59-0700_macOS_Mac_25B67.tar.gz 'Reading managed config'
```

## What the Script Does

1. **Validates Input** - Checks that the sysdiagnose file exists
2. **Extracts Archive** - Unpacks the tar.gz to a temporary directory
3. **Locates Logarchive** - Finds the system_logs.logarchive directory
4. **Searches Logs** - Uses macOS `log show` command with predicate to search for your message
5. **Displays Results** - Shows matching log entries with timestamps and process info
6. **Reports Status** - Explicitly states if nothing was found
7. **Cleans Up** - Removes temporary files

## Output Format

When matches are found:
```
Extracting archive to: /var/folders/.../sysdiagnose_analysis.XXXXX
Found logarchive at: .../system_logs.logarchive
Searching for: 'Reading managed config'
----------------------------------------
Timestamp                       Thread     Type        Activity             PID    TTL
2025-10-15 06:11:58.216218-0700 0x38a7     Default     0x0                  1177   0    sudo: Reading managed config
2025-10-15 06:12:28.543795-0700 0x39e0     Default     0x0                  1184   0    sudo: Reading managed config
...
----------------------------------------
Found 9 matching entries
Cleaning up temporary directory...
Done!
```

When no matches found:
```
No results found for: 'your search string'
```

## Notes

- The script uses `/usr/bin/log show` command which requires proper permissions
- Temporary files are automatically cleaned up after execution
- The search is case-sensitive
- Use the full path or navigate to the script directory before running
