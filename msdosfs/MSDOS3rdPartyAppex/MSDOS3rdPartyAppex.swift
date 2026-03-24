//
//  Copyright (c) 2025 Apple Inc. All rights reserved.
//
//  MSDOS3rdPartyAppex.swift
//  MSDOS3rdPartyAppex
//

import Foundation
import FSKit

@main
struct MSDOS3rdPartyAppex : UnaryFileSystemExtension {
    typealias FileSystem = FSUnaryFileSystem & FSUnaryFileSystemOperations

    var fileSystem: FSUnaryFileSystem & FSUnaryFileSystemOperations {
        msdosFileSystem()
    }
}
