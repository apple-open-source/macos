//
//  Certificate.swift
//  Security
//
//

import Security
import Foundation

@available(iOS 17.0, macOS 14.0, tvOS 17.0, watchOS 10.0, *)
public class Certificate {
    /// SecCertificate object
    public private(set) var secCertificate: SecCertificate
    public private(set) var der: Data

    /// Initialize a Certificate object given it's DER representation.
    /// throws if the passed-in data is not a valid DER-encoded X.509 certificate, otherwise creates the Certificate object
    ///
    /// - Parameters:
    ///   - derRepresentation: The DER-encoded certificate.
    /// - Throws: An error occurred while parsing the DER input
    public init(derRepresentation: Data) throws {
        guard let cert = SecCertificateCreateWithData(nil, derRepresentation as CFData) else {
            throw SwiftCertificateError.invalidParameter
        }
        self.secCertificate = cert
        self.der = derRepresentation
    }

    /// Return the DER representation of an X.509 certificate.
    ///
    /// - Parameters: none
    /// - Returns: The DER encoded X.509 certificate.
    public var derRepresentation: Data {
        return SecCertificateCopyData(self.secCertificate) as Data
    }

    /// Return a string representing summary.
    ///
    /// - Returns: If found, a string representing the Summary else nil
    public var subjectSummary: String? {
        return SecCertificateCopySubjectSummary(self.secCertificate) as String?
    }

    /// Returns the common name of the subject of a given certificate.
    ///
    /// - Returns: If found, returns a String representing the common name, else nil
    /// - Throws: an error if CommonName is not found in certificate
    public var commonName: String? {
        get throws {
            var commonName: CFString?
            let ret = SecCertificateCopyCommonName(self.secCertificate, &commonName)
            if ret != 0 {
                throw SwiftCertificateError.noCommonNameInCertificate
            }
            if let swName = commonName as String? {
                return swName
            }
            return nil
        }
    }

    /// Returns an array of zero or more email addresses for the subject of a given certificate.
    ///
    /// - Returns: Returns an array of 0 or more email addresses
    /// - Throws: an error if email address is not found in certificate
    public var emailAddresses: [String] {
        get throws {
            var emailAddCF: CFArray?
            let ret = SecCertificateCopyEmailAddresses(self.secCertificate, &emailAddCF)
            if ret != 0 {
                throw SwiftCertificateError.noEmailAddressInCertificate
            }
            let emailSw = emailAddCF as! [String]
            return emailSw
        }
    }

    /// Return the certificate's normalized issuer. The content returned is a DER-encoded X.509 distinguished name.
    ///
    /// - Returns: If found, returns a DER-encoded X.509 distinguished name
    /// - Throws: an error if unable to get the issuer name
    public var normalizedIssuerSequence: Data {
        get throws {
            guard let nis = SecCertificateCopyNormalizedIssuerSequence(self.secCertificate) as Data? else {
                throw SwiftCertificateError.invalidCertificate
            }
            return nis
        }
    }

    /// Return the certificate's normalized subject
    ///
    /// - Returns: If found, returns a DER-encoded X.509 distinguished name
    /// - Throws: an error if unable to get the subject name
    public var normalizedSubjectSequence: Data {
        get throws {
            guard let nss = SecCertificateCopyNormalizedSubjectSequence(self.secCertificate) as Data? else {
                throw SwiftCertificateError.invalidCertificate
            }
            return nss
        }
    }

    /// Retrieves the public key for a given certificate. Exports SecKey type to an external representation suitable to key type.
    /// - Returns: If found, returns the pubic key
    /// - Throws: an error if unable to get the public key from the certificate
    public var publicKey: Data {
        get throws {
            var cferror: Unmanaged<CFError>?
            if let secKey = SecCertificateCopyKey(self.secCertificate) {
                let publicKeyData = SecKeyCopyExternalRepresentation(secKey, &cferror) as Data?
                guard let publicKeyData = publicKeyData, cferror == nil else {
                    let error = cferror?.takeRetainedValue()
                    guard let error = error else {
                        throw SwiftCertificateError.invalidCertificate
                    }
                    throw SwiftCertificateError.underlyingSecurityError(error: error)
                }
                return publicKeyData
            }
            throw SwiftCertificateError.invalidCertificate
        }
    }

    /// Return the certificate's serial number.
    /// - Returns: If found, returns the public key
    /// - Throws: an error if unable to get the public key from the certificate
    public var serialNumberData: Data {
        get throws {
            var cferror: Unmanaged<CFError>?
            let serial = SecCertificateCopySerialNumberData(self.secCertificate, &cferror) as Data?
            guard let serial = serial, cferror == nil else {
                let error = cferror?.takeUnretainedValue()
                guard let error = error else {
                    throw SwiftCertificateError.invalidCertificate
                }
                throw SwiftCertificateError.underlyingSecurityError(error: error)
            }
            return serial
        }
    }

 }

@available(iOS 17.0, macOS 14.0, tvOS 17.0, watchOS 10.0, *)
public class DigitalIdentity: Certificate {

    /// The private key associated with the certificate
    public private(set) var privateKey: SecKey

    /// Initialize a DigitalIdentity object representing a Certificate and its private key
    /// throws if the passed-in data is not a valid DER-encoded X.509 certificate, otherwise creates the Certificate object
    ///
    /// - Parameters:
    ///   - derRepresentation: The DER-encoded certificate.
    ///   - privateKey: Private key
    /// - Throws: An error occurred while parsing the DER input
    public init(certificate: Certificate, privateKey: SecKey) throws {
        self.privateKey = privateKey;
        try super.init(derRepresentation: certificate.der)
    }
}
