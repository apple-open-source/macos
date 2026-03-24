@macos
Feature: SecurityAgent 3rd-party plugins
  3rd-party plugins are stored in /Library/Security/SecurityAgentPlugins, as a .bundle files.
  For testing purposes, we will use NameAndPassword plugin which is stored in the QA/configuration folder

  Background:
    # /Library/Security/SecurityAgentPlugins
    Given plugin is stored in SecurityAgent plugins folder
    And FileVault is disabled
    # Scenarios below are modifying system.login.console right - there is a chance that you will make the device unable to land to LoginWindow or login if something goes wrong
    # Make sure that remote login is enabled:
    # SysPrefs -> Sharing -> Remote login enabled
    # Also, get the IP address of the device, as the domain address won't usually work in the corrupted state. You can find the IP in SysPrefs -> Network
    # In case something goes wrong, SSH to the device 'user@IP_address' and modify authdb back to it's original state:
    # $ sudo security authorizationdb write system.login.console <loginOrig.plist
    And remote login through SSH is enabled on the test device

  Scenario: Plugin is working in a standalone defined right
    # nameandpassword.plist
    #
    # <?xml version="1.0" encoding="UTF-8"?>
    # <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    # <plist version="1.0">
    # <dict>
    # <key>class</key>
    # <string>evaluate-mechanisms</string>
    # <key>mechanisms</key>
    # <array>
    # <string>NameAndPassword:invoke</string>
    # <string>builtin:authenticate,privileged</string>
    # </array>
    # <key>shared</key>
    # <false/>
    # </dict>
    # </plist>
    #
    # Define new right:
    # $ sudo security authorizationdb write NameTestRight <nameandpassword.plist
    Given Standalone right is defined in authdb
    # $ security authorize -u NameTestRight
    # You will probably be alerted that the software is malicious - you have to go to the SysPrefs -> Security & Privacy and click "allow" on the bottom of the panel
    # After that, you will be again prompted when you call `security authorize` - now just click Open on the prompt dialog. You shouldn't be ever prompted since then when authorizing using this plugin
    And I authorize the new right
    # You should see unique UI, not similiar to anything you know from the system
    When I enter correct user credentials
    # security authorize returns:
    # YES (0)
    Then the authorization ends succesfully

  Scenario: Plugin used as part of login process in LoginWindow
    # $ security authorizationdb read system.login.console > loginOrig.plist
    # In the loginOrig.plist, modify 'loginwindow:login' to 'NameAndPassword:invoke' in the 'Mechanism' array. Save the file as loginModified.plist
    # $ sudo security authorizationdb write system.login.console <loginModified.plist
    Given I modify the 'system.login.console' to use 3rd party plugin
    And I reboot
    # You can check that the plugin is used when the password field is wide and doesn't fit the LoginWindow regular UI
    And I land to LoginWindow
    When I enter user credentials
    # No logs like this should be present in the logs:
    # (com.apple.CryptoTokenKit.ahp.agent): launchd.development[1]: Service did not exit 5 seconds after SIGTERM. Sending SIGKILL.
    Then I succesfully login

  @core
  Scenario: SIP-protected plugins are not staged
    # $ sudo darwinup -f NameAndPasswordAuthPluginSystem.tar.gz
    Given Plugin is installed under SIP path
    # $ sudo security authorizationdb write NameTestRight <com.apple.nap.plist
    And Right which uses the plugin is defined in authdb
    # $ security authorize -u NameTestRight
    When Right which uses the plugin is being authorized
    # /Library/Security/SecurityAgentPlugins/StagedPlugins
    Then Plugin has not been copied to Staging directory 
    # $ sudo log stream --info --debug --predicate 'subsystem="com.apple.Authorization"'
    # Look for '.bundle' and check that bundle is being loaded from correct directory
    # Process name should be SecurityAgent
    And Logs verify that plugin is not staged

  @core
  Scenario: Verify Plugin Staging and Unstaging
    # $ sudo security authorizationdb write NameTestRight <com.apple.nap.plist
    Given Right which uses the plugin is defined in authdb
    # $ sudo darwinup install -f NameAndPassword.tar.gz
    And NameAndPasswordAuthPluginLocal is installed
    # $ security authorize -u NameTestRight
    When Right which uses the plugin is authorized
    # /Library/Security/SecurityAgentPlugins/StagedPlugins
    And Verify that plugin is copied to Staging Directory
    Then Authorization process completes
    And Plugin is removed from the staging directory

  Scenario: Verify Cleanup on authd restart
    # ditto /path/to/dummy/file /Library/Security/SecurityAgentPlugins/StagedPlugins
    Given A dummy file/bundle is placed into Staging Directory
    # sudo killall authd
    When authd process is restarded
    Then dummy file/bundle has been removed upon auth's restart

  Scenario: Verify Cleanup on authd restart
    # $ sudo security authorizationdb write NameTestRight <com.apple.nap.plist
    Given Right which uses the plugin is defined in authdb
    # $ sudo darwinup install -f NameAndPassword.tar.gz
    And NameAndPasswordAuthPluginLocal plugin is installed
    # $ security authorize -u NameTestRight
    When Right which uses the plugin is being authorized
    And Verify that plugin is copied to Staging Directory
    # sudo killall authd
    Then authd process is restarded
    And Staged plugin has been removed from the Staging Directory 
