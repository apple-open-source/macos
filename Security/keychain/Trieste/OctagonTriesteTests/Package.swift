// swift-tools-version:4.2
import PackageDescription

let package = Package(
    name: "OctagonTrieste",
    products: [
        .library(name: "OctagonTrieste", targets: ["OctagonTrieste"]),
        .library(name: "OctagonTriesteTests", targets: ["OctagonTriesteTests"]),
        ],
    dependencies: [
        .package(url: "git@github.pie.apple.com:trieste/TriesteKit.git", .branch("master")),
        .package(path: "../OctagonTestHarnessXPCServiceProtocol"),
        ],
    targets: [
        .target(
            name: "OctagonTrieste",
            dependencies: [
                "OctagonTestHarnessXPCServiceProtocol",
                ]),
        .testTarget(
            name: "OctagonTriesteTests",
            dependencies: [
                "CloudDeviceTest",
                "CoreDeviceAutomation",
                "OctagonTestHarnessXPCServiceProtocol",
                ]),
        ]
)
