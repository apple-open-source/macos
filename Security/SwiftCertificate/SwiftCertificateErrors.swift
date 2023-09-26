//
//  SwiftCertificateErrors.swift
//  SwiftCertificate
//

import Foundation
/// Errors thrown in SwiftCertificate
public enum SwiftCertificateError: Error {
    case invalidParameter
    case incorrectParameterSize
    case invalidCertificate
    case noCommonNameInCertificate
    case noEmailAddressInCertificate
    case underlyingSecurityError(error: CFError)
}
