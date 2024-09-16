import Foundation

struct CuttlefishErrorMatcher {
    let code: CuttlefishErrorCode
}

// Use a 'pattern match operator' to make pretty case statements matching Cuttlefish errors
func ~= (pattern: CuttlefishErrorMatcher, value: Error?) -> Bool {
    guard let error = value else {
        return false
    }
    let nserror = error as NSError
    return nserror.isCuttlefishError(pattern.code)
}

// This function is only used by RetryingCKCodeService, which enforces a minimum
// retry interval of five seconds and a maximum time of 30 seconds. This means that
// -[NSError(Octagon) retryInterval], which defaults to 30 seconds, cannot be used.
// Instead, use -[NSError(Octagon) cuttlefishRetryAfter] to get the true value.
func CuttlefishRetryAfter(error: Error?) -> TimeInterval {
    guard let error = error else {
        return 0
    }
    let nserror = error as NSError
    return nserror.cuttlefishRetryAfter()
}
