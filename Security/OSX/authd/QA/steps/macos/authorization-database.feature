@macOS
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
@smoke
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

  # Rights which can't be obsolete on GoldenGateG & Star: 
    # com.apple.trust-settings.admin
  #  This can be simulated by passing obsolete definition of com.apple.trust-settings.admin (from Jazz) to the authdb: 
  #  'security authorizationdb write com.apple.trust-settings.admin < /tmp/com.apple.trust-settings.admin.obsolete'
  #
  #  com.apple.trus-settings.admin.obsolete definition:
  #
  #  <?xml version="1.0" encoding="UTF-8"?>
  #  <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
  #  <plist version="1.0">
  #  <dict>
  #      <key>class</key>
  #      <string>user</string>
  #      <key>comment</key>
  #      <string>For modifying Trust Settings in the Local Admin domain.</string>
  #      <key>created</key>
  #      <real>646327325.985734</real>
  #      <key>modified</key>                  
  #      <real>646327325.985734</real>        
  #      <key>allow-root</key>                
  #      <true/>                              
  #      <key>authenticate-user</key>
  #      <true/>
  #      <key>group</key>
  #      <string>admin</string>
  #      <key>password-only</key>
  #      <false/>
  #      <key>session-owner</key>
  #      <false/>
  #      <key>shared</key>
  #      <false/>
  #      <key>timeout</key>
  #      <integer>2147483647</integer>
  #      <key>version</key>                   
  #      <integer>0</integer>
  #  </dict>
  #  </plist> 
  Scenario: Authorization Database must be recreated if any of specified rights are obsolete  
    Given any of the specified right is obsolete
    When the machine reboots or authd restarts
    # You can check that the database recreated successfully in logs: 
    # authd: Old and not updated database found: old com.apple.trust-settings.admin right is defined
    # authd: Database is in a bad shape, recreating it
    # This process should finish succesfully: 
    # authd: Database recheck result: 0
    Then "authdb" is recreated with the updated right