// swift-tools-version: 5.7
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let buildSettings: [CXXSetting] = [
    .define("DEBUG", to: "1", .when(configuration: .debug)),
    .define("U_SHOW_CPLUSPLUS_API", to: "1"),
    .define("U_SHOW_INTERNAL_API", to: "1"),
    .define("U_TIMEZONE", to: "timezone"),
    .define("U_TIMEZONE_PACKAGE", to: "\"icutz44l\""),
    .define("FORTIFY_SOURCE", to: "2"),
    .define("STD_INSPIRED"),
    .define("MAC_OS_X_VERSION_MIN_REQUIRED", to: "101500"),
    .define("U_HAVE_STRTOD_L", to: "1"),
    .define("U_HAVE_XLOCALE_H", to: "1"),
    .define("U_HAVE_STRING_VIEW", to: "1"),
    .define("U_DISABLE_RENAMING", to: "1"),
    .define("U_COMMON_IMPLEMENTATION"),
    // Where data are stored
    .define("ICU_DATA_DIR", to: "\"/usr/share/icu/\""),
    .define("U_TIMEZONE_FILES_DIR", to: "\"/var/db/timezone/icutz\""),
    .define("USE_PACKAGE_DATA", to: "1", .when(platforms: [.linux]))
]

let commonBuildSettings: [CXXSetting] = buildSettings.appending([
    .headerSearchPath("."),
])

let i18nBuildSettings: [CXXSetting] = buildSettings.appending([
    .define("U_I18N_IMPLEMENTATION"),
    .define("SWIFT_PACKAGE", to: "1", .when(platforms: [.linux])),
    .headerSearchPath("../common"),
    .headerSearchPath("."),
])

let ioBuildSettings: [CXXSetting] = buildSettings.appending([
    .define("U_IO_IMPLEMENTATION"),
    .headerSearchPath("../common"),
    .headerSearchPath("../i18n"),
    .headerSearchPath("."),
])

let stubDataBuildSettings: [CXXSetting] = buildSettings.appending([
    .headerSearchPath("../common"),
    .headerSearchPath("../i18n"),
    .headerSearchPath("."),
])

let package = Package(
    name: "FoundationICU",
    products: [
        .library(
            name: "FoundationICU",
            targets: ["FoundationICU"]),
    ],
    targets: [
        .target(
            name: "FoundationICU",
            dependencies: [
                "ICUCommon",
                "ICUI18N",
                "ICUIO",
                "ICUStubData"
            ],
            path: "swift/FoundationICU",
            resources: [
                .copy("icudt70l.dat"),
            ]),
        .target(
            name: "ICUCommon",
            path: "icuSources/common",
            publicHeadersPath: "include",
            cxxSettings: commonBuildSettings),
        .target(
            name: "ICUI18N",
            dependencies: ["ICUCommon"],
            path: "icuSources/i18n",
            publicHeadersPath: "include",
            cxxSettings: i18nBuildSettings),
        .target(
            name: "ICUIO",
            dependencies: ["ICUCommon", "ICUI18N"],
            path: "icuSources/io",
            publicHeadersPath: "include",
            cxxSettings: ioBuildSettings),
        .target(
            name: "ICUStubData",
            dependencies: ["ICUCommon"],
            path: "icuSources/stubdata",
            publicHeadersPath: "include",
            cxxSettings: stubDataBuildSettings),
    ],
    cxxLanguageStandard: .cxx11
)

fileprivate extension Array {
    func appending(_ other: Self) -> Self {
        var me = self
        me.append(contentsOf: other)
        return me
    }
}