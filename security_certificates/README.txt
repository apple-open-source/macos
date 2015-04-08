Building Root Trust Store Assets
--------------------------------

The security_certificates project builds a set of data files into a signed bundle. These files contain the set of root CA certificates which will be trusted by the operating system, as well as mapping extended validation (EV) identifiers to their allowed roots.


1. Add (or move) certificate files to the appropriate subdirectory:

    certificates/certs
        -- intermediate CA certificates, shipped on OS X in the SystemCACertificates keychain; these are not explicitly trusted, and only exist to help complete certificate chains for enterprise customers.

    certificates/distrusted
        -- also known as "graylisted" certificates, these are marked explicitly as having no trust, even though they may chain to a trusted root. Evaluation of a distrusted certificate results in a recoverable trust failure; the user must decide whether to trust them for any particular purpose.

    certificates/removed
        -- certificates which have been removed due to expiration or deprecation, and are no longer trusted.

    certificates/revoked
        -- also known as "blacklisted" certificates, these are marked explicitly as revoked. Evaluation of a revoked certificate should always result in a non-recoverable trust failure.

    certificates/roots
        -- trusted root certificates. The contents of this directory will be the list of trusted roots in the store.


	Note: by convention, certificate files should be in DER format, and named with a .cer suffix.


2. Edit the EV configuration file to add (or remove) OID mappings for these certificates, if needed:

    certificates/evroot.config
        -- the format of each uncommented line is an EV OID followed by one or more filenames in certificates/roots which are permitted to anchor EV chains for that OID.


3. Edit the trust store version:

    The trust store version is a number in the format YYYYMMDDNN, where YYYY is the year, MM is the month, DD is the day, and NN is the build number. By convention, NN=00 for base builds that will ship with an OS release, and a non-zero value for asset builds that ship as a standalone update. For example, 2015011900 indicates the trust store contents were changed on 19 Jan 2015, and this is a base build.  If the trust store is being produced as a standalone update, this could be 2015011901 instead.

    Currently, the trust store version must be manually edited in these 3 files:

    config/security_certificates.xcconfig
        -- change the value of TRUST_STORE_VERSION

    config/AssetVersion.plist
        -- change the numeric value for the VersionNumber key

    config/Info-Asset.plist
        -- change the numeric value for the ContentVersion key


4. Build the project:

    a) for OSX, build the osx_trust_store target
    b) for iOS, build the ios_trust_store target


