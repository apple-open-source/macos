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
        do {
            try fileDesc.writeAll(startChar.utf8)
        } catch {
            Logger(subsystem: "StreamingEncoder", category: "init").error("StreamingEncoder could not write initial byte: \(error)")
            throw error
        }
        // Don't set self.endChar until we're done with all potential throws, so deinit won't try to write it
        self.endChar = endChar
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

    // Convert some types that JSONSerialization doesn't know how to serialize.
    static func cleanValue(_ value: Any) -> Any {
        switch value {
        case let subDict as [AnyHashable: Any]:
            return cleanDictionaryForJSON(subDict)
        case let subArray as [Any]:
            return subArray.map(cleanValue)
        case let data as Data:
            return data.base64EncodedString()
        case let date as Date:
            return date.formatted(
                .iso8601
                .year()
                .month()
                .day()
                .timeZone(separator: .omitted)
                .time(includingFractionalSeconds: true)
                .timeSeparator(.colon))
        default:
            return JSONSerialization.isValidJSONObject([value]) ? value : String(describing: value)
        }
    }

    static func cleanDictionaryForJSON(_ d: [AnyHashable: Any]) -> [AnyHashable: Any] {
        return d.mapValues(cleanValue)
    }

    static func encode(fileDesc: FileDescriptor, obj: Any) throws {
        let objToEncode = cleanValue(obj)
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

    func append(key: String, value: Any?) throws {
        guard let value else {
            return
        }
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
