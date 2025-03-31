@macos @smoke
Feature: Authorization Database stores definitions of all user authentication rights
  "Authorization Database" is built from /System/Library/Security/Authorization.plist which contains all the rights in a readable form and which get's updated if right definition changes.
  "authdb" is stored in '/var/db/auth.db' and can be modified using 'security authorizationdb' cli command.

  ################################################################
  How to use 'security authorizationdb', taken from 'man security'

  authorizationdb read <right-name>
  authorizationdb write <right-name> [allow|deny|<rulename>]
  authorizationdb remove <right-name> Read/Modify authorization policy database. Without a rulename write will read a dictionary as a plist from stdin.

  Examples
  'security> security authorizationdb read system.privilege.admin > /tmp/aewp-def'
  Read definition of system.privilege.admin right.

  'security> security authorizationdb write system.preferences < /tmp/aewp-def'
  Set system.preferences to definition of system.privilege.admin right.

  'security> security authorizationdb write system.preferences authenticate-admin'
  Every change to preferences requires an Admin user to authenticate.

  # Mandatory rights GoldenGateG & Star builds:
  # com.apple.installassistant.requestpassword
  # com.apple.system-migration.launch-password
  # com.apple.trust-settings.admin
  @automatable @core @5m
  Scenario: Authorization Database must be recreated if any of the mandatory right is missing
    # Can be simulated by calling eg. 'security authorizationdb remove com.apple.system-migration.launch-password'
    Given any of the mandatory rights is missing
    When the machine reboots or authd restarts
    # You can check that the database recreated successfully in logs:
    # authd: Old and not updated database found: no com.apple.system-migration.launch-password right is defined
    # authd: Database is in a bad shape, recreating it
    # This process should finish succesfully:
    # authd: Database recheck result: 0
    Then "authdb" is recreated including the missing right
