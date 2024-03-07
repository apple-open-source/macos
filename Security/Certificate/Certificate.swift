//
//  Certificate.swift
//  Security
//
//

import Foundation

struct Certificate {
    var secCertificate: UnsafeMutablePointer<SecCertificate>
    var privateKey: Data

    init(cert: Data) { }
    init(cert: Data, privateKey: Data) { }

    /// Return the DER representation of an X.509 certificate.
    ///
    ///
    func copyData() -> Data { }

    /// Return a string representing summary.
    ///
    ///
    func subjectSummary() -> String { }

    /// Returns the common name of the subject of a given certificate.
    ///
    ///
    func commonName() throws -> [String] { }

    /// Returns an array of zero or more email addresses for the subject of a given certificate.
    ///
    func emailAddresses() throws -> [String] { }

    /// Return the certificate's normalized issuer
    ///
    func normalizedIssuerSequence() -> Data { }

    /// Return the certificate's normalized subject
    ///
    func normalizedSubjectSequence() -> Data { }

    /// Retrieves the public key for a given certificate.
    ///
    func publicKey() throws -> Data { }

    /// Return the certificate's serial number.
    ///
    func serialNumberData() throws -> Data { }

    /// Returns the private key associated with an identity.
    ///
    func copyPrivateKey() throws -> Data {}
 }
