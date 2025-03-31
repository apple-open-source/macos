@automatable @macos @smoke @core
Feature: Authorization-driven screen unlock in MacOS
  Screen unlock is driven by the 'Authorization' since macOS Rome. Unlike usual authorizations, this one is divided between LoginWindow and the Authorization. LoginWindow displays UI and gathers credentials
  while Authorization evaluates provided credential. The right authorized for the screen unlock is 'system.login.screensaver'.

  Background:
    Given there is at least two administrator user accounts
    And there is at least two standard user accounts

  @1m
  Scenario: Standard scrensaver unlock - Wrong credentials
    Given I lock the screen from Apple menu
    When I enter incorrect/empty user credentials
    Then the passfield shakes and screen remains locked

  @1m
  Scenario: Standard screensaver unlock - Correct credentials
    Given I lock the screen from Apple menu
    When I enter logged user credentials
    Then the screen gets unlocked
    And the user keybag is unlocked

  @1m
  Scenario: Standard screensaver unlock - Correct credentials and keypress
    Given I am logged as std1
    And lock the screen from Apple menu
    When I press Option+Return
    And I enter std1 credentials
    Then the screen gets unlocked
    And the user keybag is unlocked

  @1m
  Scenario: Standard screensaver unlock - User cannot unlock other user
    Given I am logged as std1
    And lock the screen from Apple menu
    When I press Option+Return
    And I enter std2 credentials
    Then the passfield shakes and screen remains locked

  @1m
  Scenario: Standard screensaver unlock - Admin can unlock other user
    Given I am logged as std1
    And lock the screen from Apple menu
    When I press Option+Return
    And I enter any admin credentials
    Then the screen gets unlocked

  @1m
  Scenario: Standard screensaver unlock - Admin cannot unlock other admin
    Given I am logged as admin1
    And lock the screen from Apple menu
    When I press Option+Return
    Then username field does not appear and you cant use credentials of other user

  @1m
  Scenario: Standard screensaver unlock - User cannot unlock other admin
    Given I am logged as admin1
    And lock the screen from Apple menu
    When I press Option+Return
    Then username field does not appear and you cant use credentials of other user

  # Line below is part of the /etc/pam.d/screensaver on clean install and admin unlock for other admins is not possible
  # Option+Return won't switch to username/pwd fields to enter other users credentials
  @1m
  Scenario: Standard screensaver unlock - Admin can unlock other admin
    #  To turn "admin can unlock admins" feature you need to edit file /etc/pam.d/screensaver and remove this line:
    #    "account    required       pam_group.so no_warn deny group=admin,wheel ruser fail_safe"
    Given I enabled "admin can unlock admins"
    And I am logged as admin1
    And lock the screen from Apple menu
    When I press Option+Return
    And I enter admin2 credentials
    Then the screen gets unlocked

  # Line below is part of the /etc/pam.d/screensaver on clean install and admin unlock for other admins is not possible
  # Option+Return won't switch to username/pwd fields to enter other users credentials
  @1m
  Scenario: Standard screensaver unlock - User cannot unlock other admin
    #  To turn "admin can unlock admins" feature you need to edit file /etc/pam.d/screensaver and remove this line:
    #    "account    required       pam_group.so no_warn deny group=admin,wheel ruser fail_safe"
    Given I enabled "admin can unlock admins"
    And I am logged as admin1
    And lock the screen from Apple menu
    When I press Option+Return
    And I enter std1 credentials
    Then the passfield shakes and screen remains locked
