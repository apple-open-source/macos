@macos @smoke
Feature: AEWP - Authorization Execute With Privileges
  "AEWP" provides a way to run any executable with administrator or root privileges.
  'AuthorizationExecuteWithPrivileges' function is deprecated, but a lot of 3rd party installers are still depending on it's functionality.

  Background:
    Given there is at least one administrator user account

  @automatable @core @5m
  Scenario: AEWP - AppleScript
    Given I open Terminal
    # osascript -e 'do shell script "whoami" with administrator privileges'
    And I run AppleScript with administrator privileges
    And I'm prompted with Authorization dialogue
    When I enter correct credentials
    # You should see output of "whoami" command if the command from above was used
    Then I get correct output from the command

  @notautomatable @5m
  Scenario Outline: AEWP - third-party Apps
    # Run 'apendix' (AppleInternal) - download 'Maya' & 'Reason' installer
    # Maya asks for AEWP during it's installation
    # Reason does not have an installer, instead you copy it Application folder. It asks for AEWP when you start the app
    Given I run <app>
    And I'm prompted for admin Authorization
    When I enter correct credentials
    Then installation finishes succesfully

    Examples:
      | app            |
      | Maya installer |
      | Reason         |
