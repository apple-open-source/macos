# Testing the Security project components using Trieste

## Development testing (a.k.a. Testing from your local machine while developing)

### Trieste-specific components

Security project contains several components that enable Trieste testing. Those are:

- Three projects located under `keychain/Trieste`:
    * `OctagonTestHarnessXPCServiceProtocol`: this is an XPC API exposed by the test harness for remote invocations by the Trieste tests;
    * `OctagonTestHarnessXPCService`: this is an XPC service implementating the API above using the NSXPC platform;
    * `OctagonTestHarness`: this is a container project for the XPC protocol and XPC service above.
- One project located under `keychain/Trieste/OctagonTriesteTests`:
    * `OctagonTrieste`: this is a test project that is ran by Xcode on the developer's macOS machine and invokes `OctagonTestHarness` remotely on a device.
    
Projects under `keychain/Trieste` are explicitly added to the top-level `Security.xcodeproj` and are available right after the project is open. Of those three, `OctagonTestHarnessXPCServiceProtocol` is available as a Swift Package Manager project, but not managed as such by Xcode -- it's managed as a simple directory tree of sources.

Unlike these projects above, `OctagonTrieste` is a real Swift Package Manager project that is fully managed by Swift. Because of that, this project's `.xcodeproj` file is added as a sub-project to `Security.xcodeproj`, but is not checked into the source control.

This means that after cloning the repository, but before opening `Security.xcodeproj`, the following command needs to be run:

```
cd keychain/Trieste/OctagonTriesteTests
./remake-local-project.sh
```

This will generate `OctagonTrieste.xcodeproj` that is already set up to be referenced from `Security.xcodeproj`. The commands above need to be re-run when there are some major changes to the OctagonTrieste project, like adding or removing sources files, or updating or modifying dependencies.

### Trieste-specific test schemes configuration

The following environment variables are needed in the `OctagonTrieste-Package` scheme's Run action:

- `TRIESTE_INSTALLED_TARGETS_PROJECT_FILE_DIR=$(PROJECT_DIR)/../../..`
- `TRIESTE_SCREEN_SHARING_DO_NOT_OPEN_AUTOMATICALLY=YES`
- `TRIESTE_TEAM_ID` -- this needs to be set to your WWDR team ID. Please contact Trieste team at `trieste-dev@group.apple.com` if you have authentication troubles after setting this.
- `TRIESTE_GATEWAY_URL=https://p33-trieste.ic.apple.com`

When running the tests involving installation of local targets to a remote Trieste-controlled device (see `CDAIOSDevice.installXcodeTargets(_:rebootWhenDone:)` for details), you may get errors about Xcode scheme `OctagonTestHarness` missing from your project. This indeed may be the case and you need to add a scheme named `OctagonTestHarness` based on the target `OctagonTestHarness` to fix that. This happens because not all schemes may happen to be committed to the repository.
