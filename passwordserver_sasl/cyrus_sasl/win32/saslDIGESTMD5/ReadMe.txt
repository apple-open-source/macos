========================================================================
       DYNAMIC LINK LIBRARY : saslDIGESTMD5
========================================================================


SASL Authentication Module: DIGEST MD5

Modifications made to project settings:

* Include path 
- Added main sasl include directory

* Precompiled header
- Turned off.

* Libraries for linking
- Added winsock2 (ws2_32.lib)
- Added libsasl (libsasl.lib)

* Defined "STDC_HEADERS" to fix index() bug
