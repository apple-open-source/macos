import Foundation

struct CuttlefishErrorMatcher {
    let code: CuttlefishErrorCode
}

// Use a 'pattern match operator' to make pretty case statements matching Cuttlefish errors
func ~=(pattern: CuttlefishErrorMatcher, value: Error?) -> Bool {
    guard let error = value else {
        return false
    }
    let nserror = error as NSError
    return nserror.isCuttlefishError(pattern.code)
}

func CuttlefishRetryAfter(error: Error?) -> TimeInterval {
    guard let error = error else {
        return 0
    }
    let nserror = error as NSError
    return nserror.cuttlefishRetryAfter()
}
