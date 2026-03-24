Feature: Sudo with TouchID and Smart Card authentication
  As a system administrator
  I want to enable TouchID and Smart Card authentication for sudo
  So that users can authenticate using biometric or hardware-based security methods

  Background:
    Given the system is a macOS device with TouchID capability
    And sudo is installed and configured
    And at least one root session terminal is open (sudo -i) as a safety precaution

  @core @smoke @automatable @3m
  Scenario: Configure TouchID authentication for sudo
    Given an administrator is logged into the system
    And the file "/etc/pam.d/sudo_local" does not exist
    When the administrator runs "sudo cp /etc/pam.d/sudo_local.template /etc/pam.d/sudo_local"
    Then the file "/etc/pam.d/sudo_local" should exist
    When the sudo_local configuration file is made writable by "sudo chmod 777 /etc/pam.d/sudo_local"
    And verifies that "auth sufficient pam_tid.so" is uncommented at the top of "/etc/pam.d/sudo_local"
    Then TouchID should be configured for sudo authentication

  @core @smoke @automatable @1m
  Scenario: Test sudo with TouchID authentication
    Given TouchID is configured for sudo authentication
    When the user runs "sudo -k whoami"
    Then the system should prompt for TouchID authentication
    When the user provides valid TouchID authentication
    Then the command should execute successfully
    And the output should show "root"

  @smoke @automatable @1m
  Scenario: TouchID fallback to password after failed attempts
    Given TouchID is configured for sudo authentication
    When the user runs "sudo -k whoami"
    Then the system should prompt for TouchID authentication
    When the user provides invalid TouchID authentication 3 times
    Then the system should fall back to password authentication
    When the user enters their correct password
    Then the command should execute successfully
    And the output should show "root"

  @core @smoke @notautomatable @3m
  Scenario: Test sudo with TouchID and Smart Card PIN dialog
    Given TouchID is configured for sudo authentication
    And Smart Card authentication is configured for sudo
    And a valid Smart Card is inserted
    When the user runs "sudo -k whoami"
    Then the system should display a dialog with TouchID and PIN options
    When the user selects PIN authentication and enters the correct PIN
    Then the command should execute successfully
    And the output should show "root"

  @core @smoke @notautomatable @3m
  Scenario: Test sudo with Smart Card PIN in command line (TouchID removed)
    Given Smart Card authentication is configured for sudo
    And TouchID configuration is removed from "/etc/pam.d/sudo_local"
    And a valid Smart Card is inserted
    When the user runs "sudo -k whoami"
    Then the system should prompt for Smart Card PIN in the command line
    When the user enters the correct PIN
    Then the command should execute successfully
    And the output should show "root"

  @automatable @5m
  Scenario: Test sudo with askpass-enabled in pam_tid
    Given TouchID is configured for sudo authentication
    When the administrator runs an askpass script that returns password
    # The script contains "#!/bin/zsh" and "osascript -e 'Tell application \"System Events\" to display dialog \"Enter password:\" default answer \"\" with hidden answer' | awk -F': ' '/text returned:/ {print $2}'" (this is an example)
    # The script is available as askpass_script.sh
    And exports the script path as "export SUDO_ASKPASS=<script path>"
    When the user runs "sudo ls /var/root"
    Then the system should show TouchID prompt for enrolled finger
    When the user runs "sudo -A ls /var/root"
    Then sudo should not ask for authorization, password or TouchID
    And the command should execute using the askpass script
