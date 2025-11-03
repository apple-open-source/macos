Feature: Normal sudo operations
              As a system administrator
              I want to control sudo access for different user types
  So that security policies are properly enforced

        Background:
            Given the system has sudo installed and configured
              And both an administrator and a standard user are available on the system
              And at least one root session terminal is open (sudo -i) as a safety precaution

        @core @automatable @1m
        Scenario: Administrator successfully executes sudo command
            Given an administrator is logged into the system
             When the administrator runs "sudo whoami"
              And enters their correct password
             Then the command should execute successfully
              And the output should show "root"

        @core @automatable @1m
        Scenario: Administrator enters incorrect password for sudo
            Given an administrator is logged into the system
             When the administrator runs "sudo whoami"
              And enters an incorrect/empty password
             Then the command should fail
              And an authentication error should be displayed
              And the command should not execute

        @core @automatable @1m
        Scenario: Standard user switches to administrator and executes sudo command
            Given a standard user is logged into the system
              And an administrator account exists on the system
             When the standard user runs "su - admin"
              And enters the administrator's login password
             Then the user should be switched to the administrator account
             When the administrator runs "sudo whoami"
              And enters their correct password
             Then the command should execute successfully
              And the output should show "root"

        @core @automatable @1m
        Scenario: Standard user without sudo privileges attempts sudo command
            Given a standard user is logged into the system
              And the standard user is not in sudoers or wheel group
             When the standard user runs "sudo whoami"
              And enters their password
             Then the command should fail
              And an error message should be displayed indicating the user is not in the sudoers file
              And the user should not be able to execute the command as root