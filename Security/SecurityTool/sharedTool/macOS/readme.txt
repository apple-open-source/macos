To test security2, you must add --platform-identifer=5 to "Other Code Signing Flags" in Xcode Build Settings for the security2_tool target for it to pass AMFI muster, even when defanged. See rdar://80497763

You must also put the binary at /usr/local/bin/security2 for it to pass the SecTaskIsEligiblePlatformBinary check (e.g. using darwinup).
