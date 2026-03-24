Feature: Sudo with cached credentials and force password
  As a system administrator
  I want sudo to cache credentials for a limited time and force password prompts when needed
  So that users don't need to re-enter passwords frequently while maintaining security control

  Background:
    Given the system has sudo installed and configured
    And sudo credential caching is enabled with default timeout
    And at least one root session terminal is open (sudo -i) as a safety precaution

  @core @smoke @automatable @3m
  Scenario: Administrator uses cached credentials within timeout period
    Given an administrator is logged into the system
    And the administrator has not used sudo recently
    When the administrator runs "sudo whoami"
    And enters their correct password
    Then the command should execute successfully
    And the credentials should be cached
    When the administrator runs "sudo log config" within the cache timeout period
    Then the command should execute successfully without prompting for password
    And the output should show the log configuration

  @smoke @automatable @3m
  Scenario: Multiple sudo commands within cache timeout
    Given an administrator is logged into the system
    When the administrator runs "sudo whoami"
    And enters their correct password
    Then the command should execute successfully
    When the administrator runs "sudo date" within the cache timeout period
    Then the command should execute successfully without prompting for password
    When the administrator runs "sudo uptime" within the cache timeout period
    Then the command should execute successfully without prompting for password
    And all commands should execute with root privileges

  @core @smoke @automatable @3m
  Scenario: Administrator forces password prompt with -k after cached credentials
    Given an administrator is logged into the system
    When the administrator runs "sudo whoami"
    And enters their correct password
    Then the command should execute successfully
    And the credentials should be cached
    When the administrator runs "sudo -k"
    Then the cached credentials should be cleared
    When the administrator runs "sudo whoami"
    Then the system should prompt for password again
    When the administrator enters their correct password
    Then the command should execute successfully

  @automatable @1m
  Scenario: Administrator uses -k with command execution
    Given an administrator is logged into the system
    And the administrator has cached credentials from previous sudo usage
    When the administrator runs "sudo -k whoami"
    Then the system should prompt for password despite cached credentials
    When the administrator enters their correct password
    Then the command should execute successfully
    And the output should show "root"
    And the credentials should be cleared after execution

  @automatable @5m
  Scenario: Administrator cached credentials expire after timeout
    Given an administrator is logged into the system
    And the administrator has previously used sudo and credentials are cached
    # sudo default cache timeout is 5 minutes (Edit sudoers: sudo visudo, add or edit: Defaults timestamp_timeout=5)
    When the cache timeout period has elapsed
    And the administrator runs "sudo whoami"
    Then the system should prompt for password again
    When the administrator enters their correct password
    Then the command should execute successfully
    And the credentials should be cached again