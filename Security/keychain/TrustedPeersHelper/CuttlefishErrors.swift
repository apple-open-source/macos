import Foundation

let CuttlefishErrorDomain = "CuttlefishError"
enum CuttlefishErrorCode: Int {
    case establishFailed = 1001
    case invalidChangeToken = 1005
    case resultGraphNotFullyReachable = 1007
    case changeTokenExpired = 1018
    case transactionalFailure = 1019
    case retryableServerFailure = 1021
    case keyHierarchyAlreadyExists = 1033
}

struct CuttlefishErrorMatcher {
    let code: CuttlefishErrorCode
}

// Use a 'pattern match operator' to make pretty case statements matching Cuttlefish errors
func ~=(pattern: CuttlefishErrorMatcher, value: Error?) -> Bool {
    guard let value = value else {
        return false
    }

    let error = value as NSError

    guard let underlyingError = error.userInfo[NSUnderlyingErrorKey] as? NSError else {
        return false
    }

    return error.domain == CKInternalErrorDomain && error.code == CKInternalErrorCode.errorInternalPluginError.rawValue &&
        underlyingError.domain == CuttlefishErrorDomain && underlyingError.code == pattern.code.rawValue
}
