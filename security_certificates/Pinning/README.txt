### WARNING: Notes about incompatibilities ###
* macOS 10.13.0 through macOS 10.13.3 (and all corresponding embedded builds) will fail all evaluations against a policy that includes unknown policy checks. So if you create a policy with a rule name that is not in SecPolicyServerInitialize and ship that asset to those devices, that policy will never pass trust evaluation.
* iOS 11.0 through 11.2.x (but NOT corresponding macOS builds) does not honor the compatibility version of the asset. Updating the compatibility version will not save you from breaking those builds.
* iOS 11.3 and macOS 10.13.4 deprecated this asset type in favor of PKITrustSupplementals.
