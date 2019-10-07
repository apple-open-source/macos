// swift-tools-version:4.0
import PackageDescription

let package = Package(
    name: "OctagonTestHarnessXPCServiceProtocol",
    products: [
        .library(name: "OctagonTestHarnessXPCServiceProtocol", targets: ["OctagonTestHarnessXPCServiceProtocol"]),
        ],
    dependencies: [],
    targets: [
        .target(
            name: "OctagonTestHarnessXPCServiceProtocol",
            dependencies: []),
        ]
)
