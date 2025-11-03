Feature: Sudoers NOPASSWD configuration
              As a system administrator
              I want to configure specific commands to run without password prompts
  So that automated processes and trusted users can execute certain commands seamlessly

  # Vi shortcuts for editing sudoers with visudo:
  # * $ vi <filename> — Open or edit a file.
  # * i — Switch to Insert mode.
  # * Esc — Switch to Command mode.
  # * :w — Save and continue editing.
  # * :wq or ZZ — Save and quit/exit vi.
  # * :q! — Quit vi and do not save changes.
  # * yy — Yank (copy) a line of text.
  # * p — Paste a line of yanked text below the current line.

        Background:
            Given the system has sudo installed and configured
              And the sudoers file can be modified
              And editing sudoers is done only by opening a terminal and running "sudo visudo"
              And at least one root session terminal is open (sudo -i) as a safety precaution
              And two admin users and one standard user are needed: admin, admin2, and stan
              And the sudoers file contains the following configuration:
                  """
                  # SUDOERS CONFIGURATION EXPLANATION:
                  # Format: user/group  hosts = (run_as_user) [NOPASSWD:|PASSWD:] commands
                  #
                  # root:   Full sudo access with password required (default behavior)
                  # admin:  Full sudo access without password (NOPASSWD: ALL)
                  # admin2: Mixed - /bin/ls without password, all other commands with password
                  #         (Note: Order matters! Specific rules must come before general ones)
                  # stan:   Full sudo access without password (same as admin)
                  #
                  # Key points:
                  # - ALL (first) = can run from any host
                  # - (ALL) = can run as any user
                  # - ALL (last) = can run any command
                  # - NOPASSWD: = no password required
                  # - PASSWD: = password required (explicit)
                  
                  root            ALL = (ALL) ALL
                  admin           ALL = (ALL) NOPASSWD: ALL
                  admin2          ALL = (ALL) NOPASSWD: /bin/ls
                  admin2          ALL = (ALL) PASSWD: ALL
                  stan            ALL = (ALL) NOPASSWD: ALL
                  """
              And the user names must not have anything in front of their name (no % symbol)
              And the user names must stay exact as specified
              And the order must be kept
              And "sudo -l" will list the configuration of the users

        @core @automatable @1m
        Scenario: Admin user executes sudo without password
            Given user "admin" is logged into the system
             When the user runs "sudo whoami"
             Then the command should execute without prompting for password
              And the output should show "root"
             When the user runs "sudo date"
             Then the command should execute without prompting for password
              And the current date and time should be displayed

        @smoke @automatable @1m
        Scenario: Admin2 user executes NOPASSWD command
            Given user "admin2" is logged into the system
             When the user runs "sudo /bin/ls"
             Then the command should execute without prompting for password
              And the command should list directory contents with root privileges

        @smoke @automatable @1m
        Scenario: Admin2 user attempts non-NOPASSWD command
            Given user "admin2" is logged into the system
             When the user runs "sudo whoami"
              And enters their correct password
             Then the command should execute successfully
              And the output should show "root"

        @smoke @automatable @3m
        Scenario: Stan user executes any command without password
            Given user "stan" is logged into the system
              And the standard user "stan" is added to the admin group
             When the user runs "sudo whoami"
             Then the command should execute without prompting for password
              And the output should show "root"
             When the user runs "sudo date"
             Then the command should execute without prompting for password
              And the current date and time should be displayed
