// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "bvh",
    platforms: [
        .macOS(.v15), .iOS(.v18)
    ],
    products: [
        .library(
            name: "bvh",
            targets: ["bvh"]),
    ],
    dependencies: [],
    targets: [
        .target(name: "bvh",
                path: ".",
                sources: ["spm.cpp"],
                publicHeadersPath: "src"),
    ],
    cxxLanguageStandard: .cxx20
)
