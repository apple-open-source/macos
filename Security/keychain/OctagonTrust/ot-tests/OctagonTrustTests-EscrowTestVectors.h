/*
* Copyright (c) 2020 Apple Inc. All Rights Reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (the 'License'). You may not use this file except in
* compliance with the License. Please obtain a copy of the License at
* http://www.opensource.apple.com/apsl/ and read it before using this
* file.
*
* The Original Code and all software distributed under the License are
* distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
* Please see the License for the specific language governing rights and
* limitations under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/

#if OCTAGON

NSString *accountInfoWithInfoSample = @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\
<plist version=\"1.0\">\
<dict>\
    <key>SecureBackupAccountIsHighSecurity</key>\
    <false/>\
    <key>SecureBackupAlliCDPRecords</key>\
    <array>\
        <dict>\
            <key>SecureBackupEscrowDate</key>\
            <date>2020-01-31T03:07:40Z</date>\
            <key>SecureBackupRemainingAttempts</key>\
            <integer>10</integer>\
            <key>encodedMetadata</key>\
            <string>YnBsaXN0MDDZAQIDBAUGBwgJCgsMDQ4PECYgVnNlcmlhbF8QEkJhY2t1cEtleWJhZ0RpZ2VzdFVidWlsZFhwZWVySW5mb18QIGNvbS5hcHBsZS5zZWN1cmViYWNrdXAudGltZXN0YW1wWGJvdHRsZUlEXkNsaWVudE1ldGFkYXRhXGVzY3Jvd2VkU1BLSV8QHVNlY3VyZUJhY2t1cFVzZXNNdWx0aXBsZWlDU0NzXEMzOVYyMDlBSjlMNU8QFMhRrcsWNmcw7dI/9uhpDUpq4FZ2VjE4QTIxNE8RBLIwggSuMYIEYTAUDA9Db25mbGljdFZlcnNpb24CAQMwKwwPQXBwbGljYXRpb25EYXRlBBgYFjIwMjAwMTMwMjI0MTI2LjI4MDA1N1owVQwQUHVibGljU2lnbmluZ0tleQRBBOW+fXyAnCMa6by/cKGf1iHkcz9VEsa6rocBXgrLGVSb7Dy4XzT7fa1jf+X2co6ZTrXr3Vt56TBJZx8X6YNMY+swWAwPQXBwbGljYXRpb25Vc2lnBEUwQwIgY4hdZ7zcWd+Ue77JKF2OK99No8MUe9f5Fg2AzLviJKcCH04d5DOYNyFk6LTzWVuHD/2uMR7zASajNdfXbpFQ578wcAwNRGV2aWNlR2VzdGFsdDFfMBMMCU1vZGVsTmFtZQwGaVBob25lMBMMCU9TVmVyc2lvbgwGMThBMjE0MBYMDENvbXB1dGVyTmFtZQwGaVBob25lMBsMFk1lc3NhZ2VQcm90b2NvbFZlcnNpb24CAQAwfAwXT2N0YWdvblB1YmxpY1NpZ25pbmdLZXkEYQSguiFhjcalFK/bQBPruMsnWzZ0qv7VtPwmhjbdCQJ4mCMY1v6+60RCEsWMs+wQ200Tv40DvCBpzRABcsM70f8tuk1Q/wMXVDQ2kVfgmIVmobzvqNLwcSBHpU44nOEnNRkwfwwaT2N0YWdvblB1YmxpY0VuY3J5cHRpb25LZXkEYQQNGdKw9D+ZMSXl2YwRidBiFyb2GI/MGdDSCFDNvvRq5ig9sJHGMKgbswKltv7gkYzgvvg51slkltO0d5nQm0Juqj3dnIh9QtbPXfUew7LGjBNJIj3IOI8DJdnPqdGee+cwggH4DBBWMkRpY3Rpb25hcnlEYXRhBIIB4jGCAd4wEAwMRXNjcm93UmVjb3JkBQAwHAwMU2VyaWFsTnVtYmVyDAxDMzlWMjA5QUo5TDUwLQwJQmFja3VwS2V5BCBmWfEWzk0k71iVH/hINYf572sP/4l/uVZaMyhNb36frzBgDAxNYWNoaW5lSURLZXkMUHlXbkk4dmROZzZFV2F5ZVcvRlA0Y0RaUnNlM0xNbjhQeGcveC9zUHpaSklTNWNzM1JLbzQvc3RPVzQ2blE5OGlObHBTSHJuUjBrZnNiUjNYMIIBGQwFVmlld3PRggEODAdBcHBsZVRWDAdIb21lS2l0DAdQQ1MtRkRFDAlQQ1MtTm90ZXMMClBDUy1CYWNrdXAMClBDUy1Fc2Nyb3cMClBDUy1QaG90b3MMC0JhY2t1cEJhZ1YwDAtQQ1MtU2hhcmluZwwMTmFub1JlZ2lzdHJ5DAxQQ1MtQ2xvdWRLaXQMDFBDUy1GZWxkc3BhcgwMUENTLU1haWxkcm9wDAxQQ1MtaU1lc3NhZ2UMDVBDUy1NYXN0ZXJLZXkMDldhdGNoTWlncmF0aW9uDA5pQ2xvdWRJZGVudGl0eQwPUENTLWlDbG91ZERyaXZlDBBBY2Nlc3NvcnlQYWlyaW5nDBBDb250aW51aXR5VW5sb2NrBEcwRQIgRLmiTIo/hgxmoOMgZEygsTzdJiHOMTI68Y8DQGgXpWICIQCHr913nsr4kFaYZd3i/ioYQum8B5KOpxFR90u1CPgPEl8QEzIwMjAtMDEtMzEgMDM6MDc6NDBfECRERDVFM0Y5Ri0zNzAyLTQ3ODktOEFDRi0yRDI4QkM4NkE5NEPcERITFBUWFxgZGhscHQ4eHR8gISIjICMlXxAWZGV2aWNlX2VuY2xvc3VyZV9jb2xvcl8QHVNlY3VyZUJhY2t1cE1ldGFkYXRhVGltZXN0YW1wXxAPZGV2aWNlX3BsYXRmb3JtXGRldmljZV9jb2xvcl8QI1NlY3VyZUJhY2t1cE51bWVyaWNQYXNzcGhyYXNlTGVuZ3RoXxAhU2VjdXJlQmFja3VwVXNlc0NvbXBsZXhQYXNzcGhyYXNlWmRldmljZV9taWRfEBRkZXZpY2VfbW9kZWxfdmVyc2lvbltkZXZpY2VfbmFtZV8QIVNlY3VyZUJhY2t1cFVzZXNOdW1lcmljUGFzc3BocmFzZV8QEmRldmljZV9tb2RlbF9jbGFzc1xkZXZpY2VfbW9kZWxRMRQAAAAAAAAAAAAAAAAAAAABEAYJXxBQeVduSTh2ZE5nNkVXYXllVy9GUDRjRFpSc2UzTE1uOFB4Zy94L3NQelpKSVM1Y3MzUktvNC9zdE9XNDZuUTk4aU5scFNIcm5SMGtmc2JSM1haaVBob25lMTAsNVZpUGhvbmUJXWlQaG9uZSA4IFBsdXNPEHgwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAATIde8QFJDJYQJa6NrxP5WDLEhPNga9732ZGyoVoKi0RnxT6aIlb/LBrRvnrdZFyGUMlSYGSY3GIgrLz3YJ0A0W4BN6YKMtsgGCDONSD5/KHRzTEAE5e3Yp26nshhMavOcJAAgAGwAiADcAPQBGAGkAcgCBAI4ArgC7ANIA2QWPBaUFzAXlBf4GHgYwBj0GYwaHBpIGqQa1BtkG7gb7Bv0HDgcQBxEHZAdvB3YHdweFCAAAAAAAAAACAQAAAAAAAAAoAAAAAAAAAAAAAAAAAAAIAQ==</string>\
            <key>label</key>\
            <string>com.apple.icdp.record</string>\
            <key>metadata</key>\
            <dict>\
                <key>BackupKeybagDigest</key>\
                <data>\
                yFGtyxY2ZzDt0j/26GkNSmrgVnY=\
                </data>\
                <key>ClientMetadata</key>\
                <dict>\
                    <key>SecureBackupMetadataTimestamp</key>\
                    <string>2020-01-31 03:07:40</string>\
                    <key>SecureBackupNumericPassphraseLength</key>\
                    <integer>6</integer>\
                    <key>SecureBackupUsesComplexPassphrase</key>\
                    <true/>\
                    <key>SecureBackupUsesNumericPassphrase</key>\
                    <true/>\
                    <key>device_color</key>\
                    <string>1</string>\
                    <key>device_enclosure_color</key>\
                    <string>1</string>\
                    <key>device_mid</key>\
                    <string>yWnI8vdNg6EWayeW/FP4cDZRse3LMn8Pxg/x/sPzZJIS5cs3RKo4/stOW46nQ98iNlpSHrnR0kfsbR3X</string>\
                    <key>device_model</key>\
                    <string>iPhone 8 Plus</string>\
                    <key>device_model_class</key>\
                    <string>iPhone</string>\
                    <key>device_model_version</key>\
                    <string>iPhone10,5</string>\
                    <key>device_name</key>\
                    <string>iPhone</string>\
                    <key>device_platform</key>\
                    <integer>1</integer>\
                </dict>\
                <key>SecureBackupUsesMultipleiCSCs</key>\
                <true/>\
                <key>bottleID</key>\
                <string>DD5E3F9F-3702-4789-8ACF-2D28BC86A94C</string>\
                <key>build</key>\
                <string>18A214</string>\
                <key>com.apple.securebackup.timestamp</key>\
                <string>2020-01-31 03:07:40</string>\
                <key>escrowedSPKI</key>\
                <data>\
                MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEyHXvEBSQyWEC\
                Wuja8T+VgyxITzYGve99mRsqFaCotEZ8U+miJW/ywa0b\
                563WRchlDJUmBkmNxiIKy892CdANFuATemCjLbIBggzj\
                Ug+fyh0c0xABOXt2Kdup7IYTGrzn\
                </data>\
                <key>peerInfo</key>\
                <data>\
                MIIErjGCBGEwFAwPQ29uZmxpY3RWZXJzaW9uAgEDMCsM\
                D0FwcGxpY2F0aW9uRGF0ZQQYGBYyMDIwMDEzMDIyNDEy\
                Ni4yODAwNTdaMFUMEFB1YmxpY1NpZ25pbmdLZXkEQQTl\
                vn18gJwjGum8v3Chn9Yh5HM/VRLGuq6HAV4KyxlUm+w8\
                uF80+32tY3/l9nKOmU61691beekwSWcfF+mDTGPrMFgM\
                D0FwcGxpY2F0aW9uVXNpZwRFMEMCIGOIXWe83FnflHu+\
                yShdjivfTaPDFHvX+RYNgMy74iSnAh9OHeQzmDchZOi0\
                81lbhw/9rjEe8wEmozXX126RUOe/MHAMDURldmljZUdl\
                c3RhbHQxXzATDAlNb2RlbE5hbWUMBmlQaG9uZTATDAlP\
                U1ZlcnNpb24MBjE4QTIxNDAWDAxDb21wdXRlck5hbWUM\
                BmlQaG9uZTAbDBZNZXNzYWdlUHJvdG9jb2xWZXJzaW9u\
                AgEAMHwMF09jdGFnb25QdWJsaWNTaWduaW5nS2V5BGEE\
                oLohYY3GpRSv20AT67jLJ1s2dKr+1bT8JoY23QkCeJgj\
                GNb+vutEQhLFjLPsENtNE7+NA7wgac0QAXLDO9H/LbpN\
                UP8DF1Q0NpFX4JiFZqG876jS8HEgR6VOOJzhJzUZMH8M\
                Gk9jdGFnb25QdWJsaWNFbmNyeXB0aW9uS2V5BGEEDRnS\
                sPQ/mTEl5dmMEYnQYhcm9hiPzBnQ0ghQzb70auYoPbCR\
                xjCoG7MCpbb+4JGM4L74OdbJZJbTtHeZ0JtCbqo93ZyI\
                fULWz131HsOyxowTSSI9yDiPAyXZz6nRnnvnMIIB+AwQ\
                VjJEaWN0aW9uYXJ5RGF0YQSCAeIxggHeMBAMDEVzY3Jv\
                d1JlY29yZAUAMBwMDFNlcmlhbE51bWJlcgwMQzM5VjIw\
                OUFKOUw1MC0MCUJhY2t1cEtleQQgZlnxFs5NJO9YlR/4\
                SDWH+e9rD/+Jf7lWWjMoTW9+n68wYAwMTWFjaGluZUlE\
                S2V5DFB5V25JOHZkTmc2RVdheWVXL0ZQNGNEWlJzZTNM\
                TW44UHhnL3gvc1B6WkpJUzVjczNSS280L3N0T1c0Nm5R\
                OThpTmxwU0hyblIwa2ZzYlIzWDCCARkMBVZpZXdz0YIB\
                DgwHQXBwbGVUVgwHSG9tZUtpdAwHUENTLUZERQwJUENT\
                LU5vdGVzDApQQ1MtQmFja3VwDApQQ1MtRXNjcm93DApQ\
                Q1MtUGhvdG9zDAtCYWNrdXBCYWdWMAwLUENTLVNoYXJp\
                bmcMDE5hbm9SZWdpc3RyeQwMUENTLUNsb3VkS2l0DAxQ\
                Q1MtRmVsZHNwYXIMDFBDUy1NYWlsZHJvcAwMUENTLWlN\
                ZXNzYWdlDA1QQ1MtTWFzdGVyS2V5DA5XYXRjaE1pZ3Jh\
                dGlvbgwOaUNsb3VkSWRlbnRpdHkMD1BDUy1pQ2xvdWRE\
                cml2ZQwQQWNjZXNzb3J5UGFpcmluZwwQQ29udGludWl0\
                eVVubG9jawRHMEUCIES5okyKP4YMZqDjIGRMoLE83SYh\
                zjEyOvGPA0BoF6ViAiEAh6/dd57K+JBWmGXd4v4qGELp\
                vAeSjqcRUfdLtQj4DxI=\
                </data>\
                <key>serial</key>\
                <string>C39V209AJ9L5</string>\
            </dict>\
            <key>osVersion</key>\
            <string>18A214</string>\
            <key>peerInfoSerialNumber</key>\
            <string>C39V209AJ9L5</string>\
            <key>recordID</key>\
            <string>sNs6voV0N35D/T91SuGmJnGO29</string>\
            <key>recordStatus</key>\
            <string>valid</string>\
            <key>silentAttemptAllowed</key>\
            <true/>\
        </dict>\
    </array>\
    <key>SecureBackupContainsiCloudIdentity</key>\
    <true/>\
    <key>SecureBackupEnabled</key>\
    <true/>\
    <key>SecureBackupEscrowTrustStatus</key>\
    <integer>0</integer>\
    <key>SecureBackupRecoveryRequiresVerificationToken</key>\
    <false/>\
    <key>SecureBackupUsesRecoveryKey</key>\
    <false/>\
    <key>SecureBackupiCDPRecords</key>\
    <array>\
        <dict>\
            <key>SecureBackupEscrowDate</key>\
            <date>2020-01-31T03:07:40Z</date>\
            <key>SecureBackupRemainingAttempts</key>\
            <integer>10</integer>\
            <key>encodedMetadata</key>\
            <string>YnBsaXN0MDDZAQIDBAUGBwgJCgsMDQ4PECYgVnNlcmlhbF8QEkJhY2t1cEtleWJhZ0RpZ2VzdFVidWlsZFhwZWVySW5mb18QIGNvbS5hcHBsZS5zZWN1cmViYWNrdXAudGltZXN0YW1wWGJvdHRsZUlEXkNsaWVudE1ldGFkYXRhXGVzY3Jvd2VkU1BLSV8QHVNlY3VyZUJhY2t1cFVzZXNNdWx0aXBsZWlDU0NzXEMzOVYyMDlBSjlMNU8QFMhRrcsWNmcw7dI/9uhpDUpq4FZ2VjE4QTIxNE8RBLIwggSuMYIEYTAUDA9Db25mbGljdFZlcnNpb24CAQMwKwwPQXBwbGljYXRpb25EYXRlBBgYFjIwMjAwMTMwMjI0MTI2LjI4MDA1N1owVQwQUHVibGljU2lnbmluZ0tleQRBBOW+fXyAnCMa6by/cKGf1iHkcz9VEsa6rocBXgrLGVSb7Dy4XzT7fa1jf+X2co6ZTrXr3Vt56TBJZx8X6YNMY+swWAwPQXBwbGljYXRpb25Vc2lnBEUwQwIgY4hdZ7zcWd+Ue77JKF2OK99No8MUe9f5Fg2AzLviJKcCH04d5DOYNyFk6LTzWVuHD/2uMR7zASajNdfXbpFQ578wcAwNRGV2aWNlR2VzdGFsdDFfMBMMCU1vZGVsTmFtZQwGaVBob25lMBMMCU9TVmVyc2lvbgwGMThBMjE0MBYMDENvbXB1dGVyTmFtZQwGaVBob25lMBsMFk1lc3NhZ2VQcm90b2NvbFZlcnNpb24CAQAwfAwXT2N0YWdvblB1YmxpY1NpZ25pbmdLZXkEYQSguiFhjcalFK/bQBPruMsnWzZ0qv7VtPwmhjbdCQJ4mCMY1v6+60RCEsWMs+wQ200Tv40DvCBpzRABcsM70f8tuk1Q/wMXVDQ2kVfgmIVmobzvqNLwcSBHpU44nOEnNRkwfwwaT2N0YWdvblB1YmxpY0VuY3J5cHRpb25LZXkEYQQNGdKw9D+ZMSXl2YwRidBiFyb2GI/MGdDSCFDNvvRq5ig9sJHGMKgbswKltv7gkYzgvvg51slkltO0d5nQm0Juqj3dnIh9QtbPXfUew7LGjBNJIj3IOI8DJdnPqdGee+cwggH4DBBWMkRpY3Rpb25hcnlEYXRhBIIB4jGCAd4wEAwMRXNjcm93UmVjb3JkBQAwHAwMU2VyaWFsTnVtYmVyDAxDMzlWMjA5QUo5TDUwLQwJQmFja3VwS2V5BCBmWfEWzk0k71iVH/hINYf572sP/4l/uVZaMyhNb36frzBgDAxNYWNoaW5lSURLZXkMUHlXbkk4dmROZzZFV2F5ZVcvRlA0Y0RaUnNlM0xNbjhQeGcveC9zUHpaSklTNWNzM1JLbzQvc3RPVzQ2blE5OGlObHBTSHJuUjBrZnNiUjNYMIIBGQwFVmlld3PRggEODAdBcHBsZVRWDAdIb21lS2l0DAdQQ1MtRkRFDAlQQ1MtTm90ZXMMClBDUy1CYWNrdXAMClBDUy1Fc2Nyb3cMClBDUy1QaG90b3MMC0JhY2t1cEJhZ1YwDAtQQ1MtU2hhcmluZwwMTmFub1JlZ2lzdHJ5DAxQQ1MtQ2xvdWRLaXQMDFBDUy1GZWxkc3BhcgwMUENTLU1haWxkcm9wDAxQQ1MtaU1lc3NhZ2UMDVBDUy1NYXN0ZXJLZXkMDldhdGNoTWlncmF0aW9uDA5pQ2xvdWRJZGVudGl0eQwPUENTLWlDbG91ZERyaXZlDBBBY2Nlc3NvcnlQYWlyaW5nDBBDb250aW51aXR5VW5sb2NrBEcwRQIgRLmiTIo/hgxmoOMgZEygsTzdJiHOMTI68Y8DQGgXpWICIQCHr913nsr4kFaYZd3i/ioYQum8B5KOpxFR90u1CPgPEl8QEzIwMjAtMDEtMzEgMDM6MDc6NDBfECRERDVFM0Y5Ri0zNzAyLTQ3ODktOEFDRi0yRDI4QkM4NkE5NEPcERITFBUWFxgZGhscHQ4eHR8gISIjICMlXxAWZGV2aWNlX2VuY2xvc3VyZV9jb2xvcl8QHVNlY3VyZUJhY2t1cE1ldGFkYXRhVGltZXN0YW1wXxAPZGV2aWNlX3BsYXRmb3JtXGRldmljZV9jb2xvcl8QI1NlY3VyZUJhY2t1cE51bWVyaWNQYXNzcGhyYXNlTGVuZ3RoXxAhU2VjdXJlQmFja3VwVXNlc0NvbXBsZXhQYXNzcGhyYXNlWmRldmljZV9taWRfEBRkZXZpY2VfbW9kZWxfdmVyc2lvbltkZXZpY2VfbmFtZV8QIVNlY3VyZUJhY2t1cFVzZXNOdW1lcmljUGFzc3BocmFzZV8QEmRldmljZV9tb2RlbF9jbGFzc1xkZXZpY2VfbW9kZWxRMRQAAAAAAAAAAAAAAAAAAAABEAYJXxBQeVduSTh2ZE5nNkVXYXllVy9GUDRjRFpSc2UzTE1uOFB4Zy94L3NQelpKSVM1Y3MzUktvNC9zdE9XNDZuUTk4aU5scFNIcm5SMGtmc2JSM1haaVBob25lMTAsNVZpUGhvbmUJXWlQaG9uZSA4IFBsdXNPEHgwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAATIde8QFJDJYQJa6NrxP5WDLEhPNga9732ZGyoVoKi0RnxT6aIlb/LBrRvnrdZFyGUMlSYGSY3GIgrLz3YJ0A0W4BN6YKMtsgGCDONSD5/KHRzTEAE5e3Yp26nshhMavOcJAAgAGwAiADcAPQBGAGkAcgCBAI4ArgC7ANIA2QWPBaUFzAXlBf4GHgYwBj0GYwaHBpIGqQa1BtkG7gb7Bv0HDgcQBxEHZAdvB3YHdweFCAAAAAAAAAACAQAAAAAAAAAoAAAAAAAAAAAAAAAAAAAIAQ==</string>\
            <key>label</key>\
            <string>com.apple.icdp.record</string>\
            <key>metadata</key>\
            <dict>\
                <key>BackupKeybagDigest</key>\
                <data>\
                yFGtyxY2ZzDt0j/26GkNSmrgVnY=\
                </data>\
                <key>ClientMetadata</key>\
                <dict>\
                    <key>SecureBackupMetadataTimestamp</key>\
                    <string>2020-01-31 03:07:40</string>\
                    <key>SecureBackupNumericPassphraseLength</key>\
                    <integer>6</integer>\
                    <key>SecureBackupUsesComplexPassphrase</key>\
                    <true/>\
                    <key>SecureBackupUsesNumericPassphrase</key>\
                    <true/>\
                    <key>device_color</key>\
                    <string>1</string>\
                    <key>device_enclosure_color</key>\
                    <string>1</string>\
                    <key>device_mid</key>\
                    <string>yWnI8vdNg6EWayeW/FP4cDZRse3LMn8Pxg/x/sPzZJIS5cs3RKo4/stOW46nQ98iNlpSHrnR0kfsbR3X</string>\
                    <key>device_model</key>\
                    <string>iPhone 8 Plus</string>\
                    <key>device_model_class</key>\
                    <string>iPhone</string>\
                    <key>device_model_version</key>\
                    <string>iPhone10,5</string>\
                    <key>device_name</key>\
                    <string>iPhone</string>\
                    <key>device_platform</key>\
                    <integer>1</integer>\
                </dict>\
                <key>SecureBackupUsesMultipleiCSCs</key>\
                <true/>\
                <key>bottleID</key>\
                <string>DD5E3F9F-3702-4789-8ACF-2D28BC86A94C</string>\
                <key>bottleValid</key>\
                <string>valid</string>\
                <key>build</key>\
                <string>18A214</string>\
                <key>com.apple.securebackup.timestamp</key>\
                <string>2020-01-31 03:07:40</string>\
                <key>escrowedSPKI</key>\
                <data>\
                MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEyHXvEBSQyWEC\
                Wuja8T+VgyxITzYGve99mRsqFaCotEZ8U+miJW/ywa0b\
                563WRchlDJUmBkmNxiIKy892CdANFuATemCjLbIBggzj\
                Ug+fyh0c0xABOXt2Kdup7IYTGrzn\
                </data>\
                <key>peerInfo</key>\
                <data>\
                MIIErjGCBGEwFAwPQ29uZmxpY3RWZXJzaW9uAgEDMCsM\
                D0FwcGxpY2F0aW9uRGF0ZQQYGBYyMDIwMDEzMDIyNDEy\
                Ni4yODAwNTdaMFUMEFB1YmxpY1NpZ25pbmdLZXkEQQTl\
                vn18gJwjGum8v3Chn9Yh5HM/VRLGuq6HAV4KyxlUm+w8\
                uF80+32tY3/l9nKOmU61691beekwSWcfF+mDTGPrMFgM\
                D0FwcGxpY2F0aW9uVXNpZwRFMEMCIGOIXWe83FnflHu+\
                yShdjivfTaPDFHvX+RYNgMy74iSnAh9OHeQzmDchZOi0\
                81lbhw/9rjEe8wEmozXX126RUOe/MHAMDURldmljZUdl\
                c3RhbHQxXzATDAlNb2RlbE5hbWUMBmlQaG9uZTATDAlP\
                U1ZlcnNpb24MBjE4QTIxNDAWDAxDb21wdXRlck5hbWUM\
                BmlQaG9uZTAbDBZNZXNzYWdlUHJvdG9jb2xWZXJzaW9u\
                AgEAMHwMF09jdGFnb25QdWJsaWNTaWduaW5nS2V5BGEE\
                oLohYY3GpRSv20AT67jLJ1s2dKr+1bT8JoY23QkCeJgj\
                GNb+vutEQhLFjLPsENtNE7+NA7wgac0QAXLDO9H/LbpN\
                UP8DF1Q0NpFX4JiFZqG876jS8HEgR6VOOJzhJzUZMH8M\
                Gk9jdGFnb25QdWJsaWNFbmNyeXB0aW9uS2V5BGEEDRnS\
                sPQ/mTEl5dmMEYnQYhcm9hiPzBnQ0ghQzb70auYoPbCR\
                xjCoG7MCpbb+4JGM4L74OdbJZJbTtHeZ0JtCbqo93ZyI\
                fULWz131HsOyxowTSSI9yDiPAyXZz6nRnnvnMIIB+AwQ\
                VjJEaWN0aW9uYXJ5RGF0YQSCAeIxggHeMBAMDEVzY3Jv\
                d1JlY29yZAUAMBwMDFNlcmlhbE51bWJlcgwMQzM5VjIw\
                OUFKOUw1MC0MCUJhY2t1cEtleQQgZlnxFs5NJO9YlR/4\
                SDWH+e9rD/+Jf7lWWjMoTW9+n68wYAwMTWFjaGluZUlE\
                S2V5DFB5V25JOHZkTmc2RVdheWVXL0ZQNGNEWlJzZTNM\
                TW44UHhnL3gvc1B6WkpJUzVjczNSS280L3N0T1c0Nm5R\
                OThpTmxwU0hyblIwa2ZzYlIzWDCCARkMBVZpZXdz0YIB\
                DgwHQXBwbGVUVgwHSG9tZUtpdAwHUENTLUZERQwJUENT\
                LU5vdGVzDApQQ1MtQmFja3VwDApQQ1MtRXNjcm93DApQ\
                Q1MtUGhvdG9zDAtCYWNrdXBCYWdWMAwLUENTLVNoYXJp\
                bmcMDE5hbm9SZWdpc3RyeQwMUENTLUNsb3VkS2l0DAxQ\
                Q1MtRmVsZHNwYXIMDFBDUy1NYWlsZHJvcAwMUENTLWlN\
                ZXNzYWdlDA1QQ1MtTWFzdGVyS2V5DA5XYXRjaE1pZ3Jh\
                dGlvbgwOaUNsb3VkSWRlbnRpdHkMD1BDUy1pQ2xvdWRE\
                cml2ZQwQQWNjZXNzb3J5UGFpcmluZwwQQ29udGludWl0\
                eVVubG9jawRHMEUCIES5okyKP4YMZqDjIGRMoLE83SYh\
                zjEyOvGPA0BoF6ViAiEAh6/dd57K+JBWmGXd4v4qGELp\
                vAeSjqcRUfdLtQj4DxI=\
                </data>\
                <key>serial</key>\
                <string>C39V209AJ9L5</string>\
            </dict>\
            <key>osVersion</key>\
            <string>18A214</string>\
            <key>peerInfoSerialNumber</key>\
            <string>C39V209AJ9L5</string>\
            <key>recordID</key>\
            <string>sNs6voV0N35D/T91SuGmJnGO29</string>\
            <key>recordStatus</key>\
            <string>valid</string>\
            <key>silentAttemptAllowed</key>\
            <true/>\
        </dict>\
    </array>\
    <key>SecureBackupiCloudDataProtectionEnabled</key>\
    <false/>\
</dict>\
</plist>";

NSString *testCDPRemoteRecordContextTestVector = @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\
<plist version=\"1.0\">\
<dict>\
    <key>SecureBackupAuthenticationAppleID</key>\
    <string>anna.535.paid@icloud.com</string>\
    <key>SecureBackupAuthenticationAuthToken</key>\
    <string>EAAbAAAABLwIAAAAAF5PGvkRDmdzLmljbG91ZC5hdXRovQBx359KJvlZTwe1q6BwXvK4gQUYo2WQbKT8UDtn8rcA6FvEYBANaAk1ofWx/bcfB4pcLiXR3Y0kncELCwFCEEpqpZS+klD9AY1oT9zW6VtyOgQTZJ4mfWz103+FoMh8nLJAVpYVfM/UjsiNsLfTX+rUmevfeA==</string>\
    <key>SecureBackupAuthenticationDSID</key>\
    <string>16187698960</string>\
    <key>SecureBackupAuthenticationEscrowProxyURL</key>\
    <string>https://p97-escrowproxy.icloud.com:443</string>\
    <key>SecureBackupAuthenticationPassword</key>\
    <string>PETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPET</string>\
    <key>SecureBackupAuthenticationiCloudEnvironment</key>\
    <string>PROD</string>\
    <key>SecureBackupContainsiCDPData</key>\
    <true/>\
    <key>SecureBackupMetadata</key>\
    <dict>\
        <key>BackupKeybagDigest</key>\
        <data>\
        uZ+vDf1JWIHh+MXIi487iENG2fk=\
        </data>\
        <key>ClientMetadata</key>\
        <dict>\
            <key>SecureBackupMetadataTimestamp</key>\
            <string>2020-02-20 00:38:28</string>\
            <key>SecureBackupNumericPassphraseLength</key>\
            <integer>6</integer>\
            <key>SecureBackupUsesComplexPassphrase</key>\
            <true/>\
            <key>SecureBackupUsesNumericPassphrase</key>\
            <integer>1</integer>\
            <key>device_color</key>\
            <string>1</string>\
            <key>device_enclosure_color</key>\
            <string>1</string>\
            <key>device_mid</key>\
            <string>vsoWCkYtidlo3QGgt6jvLDfeTWqKKQwHITeUEuYM7ZoyWI6CRH/ZUqsdg1fT96TyAyxUuYUF3fjRs5b1</string>\
            <key>device_model</key>\
            <string>iPhone 8 Plus</string>\
            <key>device_model_class</key>\
            <string>iPhone</string>\
            <key>device_model_version</key>\
            <string>iPhone10,2</string>\
            <key>device_name</key>\
            <string>One</string>\
            <key>device_platform</key>\
            <integer>1</integer>\
        </dict>\
        <key>SecureBackupUsesMultipleiCSCs</key>\
        <true/>\
        <key>bottleID</key>\
        <string>0125E97E-B124-4556-881A-A355805EBE47</string>\
        <key>bottleValid</key>\
        <string>valid</string>\
        <key>build</key>\
        <string>18A230</string>\
        <key>com.apple.securebackup.timestamp</key>\
        <string>2020-02-20 00:38:28</string>\
        <key>escrowedSPKI</key>\
        <data>\
        MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEB48BVh0D++mTSm9ucXC/a5M0CxFm\
        4QfFktDjGV0Oo3z7xLSBiqxOwvzl1Vt7m45Rbfk4YnyguNan7aDzD1X6S2zU\
        HhJ8nXro1aAn8tnUX6+EGV2v4iScbkeOrWkqQoWw\
        </data>\
        <key>peerInfo</key>\
        <data>\
        MIIEyjGCBHwwFAwPQ29uZmxpY3RWZXJzaW9uAgEDMCsMD0FwcGxpY2F0aW9u\
        RGF0ZQQYGBYyMDIwMDIyMDAwMzgyMi4yODc3NjNaMFUMEFB1YmxpY1NpZ25p\
        bmdLZXkEQQQCECkiqu2q7lmL+UUs1fzJWfI7ECo+3JsbiKsrXe8yr/JJC96U\
        oOz8qsdqgJOoA9pp4oaLtdGU9gzopNdRxTU8MFsMD0FwcGxpY2F0aW9uVXNp\
        ZwRIMEYCIQCNUy7VzU5erma8k0EgvXdo0YsuSgC9T4c6w3UM30vnSwIhAKsw\
        Sz6mVOEE3cVbv8WFlF88nb54MK51cyfI3pWp3rRlMG0MDURldmljZUdlc3Rh\
        bHQxXDATDAlNb2RlbE5hbWUMBmlQaG9uZTATDAlPU1ZlcnNpb24MBjE4QTIz\
        MDATDAxDb21wdXRlck5hbWUMA09uZTAbDBZNZXNzYWdlUHJvdG9jb2xWZXJz\
        aW9uAgEAMHwMF09jdGFnb25QdWJsaWNTaWduaW5nS2V5BGEErqcLz3k64Qla\
        asOxVOz9q6jNddI5ujH1zFYjhrTzkGvRCjfgTOK6cHbEbZS6/zgpXXXNMhj4\
        jsZR+scCcbv5yvRfyaZiGAIT5KrsnEiTXygR9hYmVdRqrjmk0ZQkgXOPMH8M\
        Gk9jdGFnb25QdWJsaWNFbmNyeXB0aW9uS2V5BGEEsP2N8X9uVz2lqLgeYCFE\
        8kRIUduWj7rL0ioejawj10Qj2OqNhUGYZ0xPnnaGd0uSYFiSrMT04KYXPRqZ\
        NoExp4z9Xda9JvJXjqk2wUeo+EL/smGJmgklfcFkAlY8RheCMIICEwwQVjJE\
        aWN0aW9uYXJ5RGF0YQSCAf0xggH5MBwMDFNlcmlhbE51bWJlcgwMQzM5VjIw\
        RUtKOUtUMC0MCUJhY2t1cEtleQQg+5p8h4Sbq3rSHuA6eHvUGHlVKxvMvtSh\
        DsjZD/h9OBAwYAwMTWFjaGluZUlES2V5DFB2c29XQ2tZdGlkbG8zUUdndDZq\
        dkxEZmVUV3FLS1F3SElUZVVFdVlNN1pveVdJNkNSSC9aVXFzZGcxZlQ5NlR5\
        QXl4VXVZVUYzZmpSczViMTCCAUYMBVZpZXdz0YIBOwwEV2lGaQwHQXBwbGVU\
        VgwHSG9tZUtpdAwHUENTLUZERQwJUENTLU5vdGVzDAlQYXNzd29yZHMMClBD\
        Uy1CYWNrdXAMClBDUy1Fc2Nyb3cMClBDUy1QaG90b3MMC0JhY2t1cEJhZ1Yw\
        DAtDcmVkaXRDYXJkcwwLUENTLVNoYXJpbmcMDE5hbm9SZWdpc3RyeQwMUENT\
        LUNsb3VkS2l0DAxQQ1MtRmVsZHNwYXIMDFBDUy1NYWlsZHJvcAwMUENTLWlN\
        ZXNzYWdlDA1PdGhlclN5bmNhYmxlDA1QQ1MtTWFzdGVyS2V5DA5XYXRjaE1p\
        Z3JhdGlvbgwOaUNsb3VkSWRlbnRpdHkMD1BDUy1pQ2xvdWREcml2ZQwQQWNj\
        ZXNzb3J5UGFpcmluZwwQQ29udGludWl0eVVubG9jawRIMEYCIQC9IebK7hYO\
        N4bYG05pbavoyP3uVs2GrBTvaaxhNeO/WgIhAP/M+WSwsqzcUkg5zhT806au\
        Ax7U0AMhq4OkV2yIuQd8\
        </data>\
        <key>serial</key>\
        <string>C39V20EKJ9KT</string>\
    </dict>\
    <key>SecureBackupPassphrase</key>\
    <string>333333</string>\
    <key>SecureBackupUsesMultipleiCSCs</key>\
    <true/>\
    <key>recordID</key>\
    <string>nDF7K/s5knTXbH6/+ERe2LPFZR</string>\
</dict>\
</plist>";

NSString* CDPRecordContextSilentTestVector = @"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\
<plist version=\"1.0\">\
<dict>\
    <key>SecureBackupAuthenticationAppleID</key>\
    <string>anna.535.paid@icloud.com</string>\
    <key>SecureBackupAuthenticationAuthToken</key>\
    <string>EAAbAAAABLwIAAAAAF5PHOERDmdzLmljbG91ZC5hdXRovQDwjwm2kXoklEtO/xeL3YCPlBr7IkVuV26y2BfLco+QhJFm4VhgFZSBFUg5l4g/uV2DG95xadgk0+rWLhyXDGZwHN2V9jju3eo6sRwGVj4g5iBFStuj4unTKylu3iFkNSKtTMXAyBXpn4EiRX+8dwumC2FKkA==</string>\
    <key>SecureBackupAuthenticationDSID</key>\
    <string>16187698960</string>\
    <key>SecureBackupAuthenticationEscrowProxyURL</key>\
    <string>https://p97-escrowproxy.icloud.com:443</string>\
    <key>SecureBackupAuthenticationPassword</key>\
    <string>PETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPETPET</string>\
    <key>SecureBackupAuthenticationiCloudEnvironment</key>\
    <string>PROD</string>\
    <key>SecureBackupContainsiCDPData</key>\
    <true/>\
    <key>SecureBackupPassphrase</key>\
    <string>333333</string>\
    <key>SecureBackupSilentRecoveryAttempt</key>\
    <true/>\
    <key>SecureBackupUsesMultipleiCSCs</key>\
    <true/>\
</dict>\
</plist>";

#endif

