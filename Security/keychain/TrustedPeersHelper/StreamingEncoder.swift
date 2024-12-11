//
//  StreamingEncoder.swift
//  TrustedPeersHelper
//

import Foundation
import System

public enum StreamingEncoderError: Error {
    case invalidFileDescriptor
    case doubleFinish
}

class StreamingEncoderBase {
    fileprivate let fileDesc: FileDescriptor
    fileprivate var needCommas: Bool
    fileprivate var endChar: Character?

    fileprivate init(fileDesc: FileDescriptor, startChar: Character, endChar: Character) throws {
        if fileDesc.rawValue < 0 { throw StreamingEncoderError.invalidFileDescriptor }
        self.fileDesc = fileDesc
        needCommas = false
        self.endChar = endChar
        try fileDesc.writeAll(startChar.utf8)
    }

    deinit {
        if endChar != nil {
            do {
                try finish()
            } catch {
                fatalError("StreamingEncoderBase could not write final byte")
            }
        }
    }

    // This has a side-effect, in that after the first time it's called per instance,
    // it will append a comma to the output stream.
    fileprivate func maybeComma() throws {
        if needCommas { try fileDesc.writeAll(",".utf8) }
        needCommas = true
    }

    func finish() throws {
        guard let endChar else {
            throw StreamingEncoderError.doubleFinish
        }
        try fileDesc.writeAll(endChar.utf8)
        self.endChar = nil // must use self here as we shadowed `endChar` with the guard above
    }

    static func encode(fileDesc: FileDescriptor, obj: Any) throws {
        let objToEncode = JSONSerialization.isValidJSONObject([obj]) ? obj : String(describing: obj)
        let data: Data
        do {
            data = try JSONSerialization.data(withJSONObject: objToEncode, options: [.fragmentsAllowed, .sortedKeys])
        } catch {
            fatalError("isValidJSONObject() [obj] returned true, but got error from data(): \(error)")
        }
        try fileDesc.writeAll(data)
    }
}

class StreamingEncoderArray: StreamingEncoderBase {
    init(_ fileDesc: FileDescriptor) throws {
        try super.init(fileDesc: fileDesc, startChar: "[", endChar: "]")
    }

    func append(_ obj: Any) throws {
        try maybeComma()
        try Self.encode(fileDesc: fileDesc, obj: obj)
    }

    func descend(_ handler: (_: StreamingEncoderArray) throws -> Void) throws {
        try maybeComma()
        try handler(try StreamingEncoderArray(fileDesc))
    }

    func descend(_ handler: (_: StreamingEncoderDict) throws -> Void) throws {
        try maybeComma()
        try handler(try StreamingEncoderDict(fileDesc))
    }
}

class StreamingEncoderDict: StreamingEncoderBase {
    init(_ fileDesc: FileDescriptor) throws {
        try super.init(fileDesc: fileDesc, startChar: "{", endChar: "}")
    }

    func append(key: String, value: Any) throws {
        try maybeComma()
        try Self.encode(fileDesc: fileDesc, obj: key)
        try fileDesc.writeAll(":".utf8)
        try Self.encode(fileDesc: fileDesc, obj: value)
    }

    func descend(_ key: String, _ handler: (_: StreamingEncoderArray) throws -> Void) throws {
        try maybeComma()
        try Self.encode(fileDesc: fileDesc, obj: key)
        try fileDesc.writeAll(":".utf8)
        try handler(try StreamingEncoderArray(fileDesc))
    }

    func descend(_ key: String, _ handler: (_: StreamingEncoderDict) throws -> Void) throws {
        try maybeComma()
        try Self.encode(fileDesc: fileDesc, obj: key)
        try fileDesc.writeAll(":".utf8)
        try handler(try StreamingEncoderDict(fileDesc))
    }
}
