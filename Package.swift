// swift-tools-version: 5.8

import PackageDescription

let package = Package(
    name: "bvh",
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
