//
//  StreamingEncoderTest.swift
//  TrustedPeersHelperUnitTests
//

import System
import Testing

struct StreamingEncoderTest {

    @Test
    func badFileDesc() async throws {
        #expect(throws: StreamingEncoderError.invalidFileDescriptor, "bad fileDesc should result in a throw") { try StreamingEncoderArray(FileDescriptor(rawValue: -1)) }
        #expect(throws: StreamingEncoderError.invalidFileDescriptor, "bad fileDesc should result in a throw") { try StreamingEncoderDict(FileDescriptor(rawValue: -1)) }
    }

    @Test
    func oneString() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let theString = "Hello, world!"
        try StreamingEncoderBase.encode(fileDesc: writeEnd, obj: theString)

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
            #expect(bytesRead == theString.count + 2, "encoded length not what it should be") // 2 bytes for leading & trailing "
        }
        data.count = bytesRead
        #expect("\"\(theString)\"" == String(data: data, encoding: .utf8), "encoded JSON not what it should be")

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? String
        #expect(theString == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func arrayOfOneString() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let theArray = ["Hello, world!"]
        try StreamingEncoderBase.encode(fileDesc: writeEnd, obj: theArray)

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
            #expect(bytesRead == theArray[0].count + 4, "encoded length not what it should be") // 4 bytes for [] & ""
        }
        data.count = bytesRead
        #expect("[\"\(theArray[0])\"]" == String(data: data, encoding: .utf8), "encoded JSON not what it should be")

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String]
        #expect(theArray == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func arrayOfTwoStrings() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let theArray = ["Hello", "world"]
        try StreamingEncoderBase.encode(fileDesc: writeEnd, obj: theArray)

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
            #expect(bytesRead == theArray[0].count + theArray[1].count + 7, "encoded length not what it should be") // 7 bytes for [] & "" & ,
        }
        data.count = bytesRead
        #expect("[\"\(theArray[0])\",\"\(theArray[1])\"]" == String(data: data, encoding: .utf8), "encoded JSON not what it should be")

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String]
        #expect(theArray == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func dictWithOneString() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let theDict = ["Hello": "world!"]
        try StreamingEncoderBase.encode(fileDesc: writeEnd, obj: theDict)

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
            #expect(bytesRead == 18, "encoded length not what it should be")
        }
        data.count = bytesRead
        #expect("{\"Hello\":\"world!\"}" == String(data: data, encoding: .utf8), "encoded JSON not what it should be")

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String: String]
        #expect(theDict == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func dictWithTwoStrings() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let theDict = ["Hello": "world!", "Goodbye": "earth!!"]
        try StreamingEncoderBase.encode(fileDesc: writeEnd, obj: theDict)

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
            #expect(bytesRead == 38, "encoded length not what it should be")
        }
        data.count = bytesRead
        #expect("{\"Goodbye\":\"earth!!\",\"Hello\":\"world!\"}" == String(data: data, encoding: .utf8), "encoded JSON not what it should be")

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String: String]
        #expect(theDict == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func unencodable() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        enum Unencodable {
            case foo
        }
        try StreamingEncoderBase.encode(fileDesc: writeEnd, obj: Unencodable.foo)

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
            #expect(bytesRead == 5, "encoded length not what it should be") // 2 bytes for leading & trailing "
        }
        data.count = bytesRead
        #expect("\"foo\"" == String(data: data, encoding: .utf8), "encoded JSON not what it should be")

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? String
        #expect("foo" == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingArrayOfOneString() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderArray(writeEnd)
        try streamy.append("Hello world!")
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
            #expect(bytesRead == 16, "encoded length not what it should be")
        }
        data.count = bytesRead
        #expect("[\"Hello world!\"]" == String(data: data, encoding: .utf8), "encoded JSON not what it should be")

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String]
        #expect(["Hello world!"] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingArrayOfTwoStrings() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderArray(writeEnd)
        try streamy.append("Hello world!")
        try streamy.append("Goodbye earth!!")
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String]
        #expect(["Hello world!", "Goodbye earth!!"] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingDictWithOneString() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderDict(writeEnd)
        try streamy.append(key: "Hello", value: "world!")
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String: String]
        #expect(["Hello": "world!"] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingDictWithTwoStrings() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderDict(writeEnd)
        try streamy.append(key: "Hello", value: "world!")
        try streamy.append(key: "Goodbye", value: "earth!!")
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String: String]
        #expect(["Hello": "world!", "Goodbye": "earth!!"] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingArrayOfArrays() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderArray(writeEnd)
        try streamy.descend { sub in
            try sub.append("Hello world!")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [[String]]
        #expect([["Hello world!"]] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingArrayOfDicts() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderArray(writeEnd)
        try streamy.descend { sub in
            try sub.append(key: "Hello", value: "world!")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [[String: String]]
        #expect([["Hello": "world!"]] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingDictOfDicts() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderDict(writeEnd)
        try streamy.descend("hello") { sub in
            try sub.append(key: "world", value: "earth")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String: [String: String]]
        #expect(["hello": ["world": "earth"]] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingDictOfArrays() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderDict(writeEnd)
        try streamy.descend("hello") { sub in
            try sub.append("world")
            try sub.append("earth")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String: [String]]
        #expect(["hello": ["world", "earth"]] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingArrayOfMultiArrays() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderArray(writeEnd)
        try streamy.descend { sub in
            try sub.append("foo")
            try sub.append("bar")
        }
        try streamy.descend { sub in
            try sub.append("oof")
            try sub.append("rab")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [[String]]
        #expect([["foo", "bar"], ["oof", "rab"]] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingArrayOfMultiDicts() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderArray(writeEnd)
        try streamy.descend { sub in
            try sub.append(key: "foo", value: "bar")
        }
        try streamy.descend { sub in
            try sub.append(key: "oof", value: "rab")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [[String: String]]
        #expect([["foo": "bar"], ["oof": "rab"]] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingDictOfMultiArrays() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderDict(writeEnd)
        try streamy.descend("foo") { sub in
            try sub.append("bar")
            try sub.append("gah")
        }
        try streamy.descend("oof") { sub in
            try sub.append("rab")
            try sub.append("hag")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String: [String]]
        #expect(["foo": ["bar", "gah"], "oof": ["rab", "hag"]] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func buildingDictOfMultiDicts() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderDict(writeEnd)
        try streamy.descend("foo") { sub in
            try sub.append(key: "bar", value: "gah")
        }
        try streamy.descend("oof") { sub in
            try sub.append(key: "rab", value: "hag")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String: [String: String]]
        #expect(["foo": ["bar": "gah"], "oof": ["rab": "hag"]] == roundTrip, "round trip should result in the expected output")
    }

    @Test
    func heterogenousArray() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderArray(writeEnd)
        try streamy.append("top")
        try streamy.descend { sub in
            try sub.append("array")
        }
        try streamy.descend { sub in
            try sub.append(key: "key", value: "value")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try #require(try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [Any], "encoded JSON should coerce to [Any]")
        #expect(roundTrip.count == 3, "round trip should result in expected number of array items")
        #expect(roundTrip[0] as? String == "top", "round trip should result in expected 0th item")
        #expect(roundTrip[1] as? [String] == ["array"], "round trip should result in expected 1st time")
        #expect(roundTrip[2] as? [String: String] == ["key": "value"], "round trip should result in expected 2nd item")
    }

    @Test
    func heterogenousDict() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }

        let streamy = try StreamingEncoderDict(writeEnd)
        try streamy.append(key: "one", value: "top")
        try streamy.descend("two") { sub in
            try sub.append("array")
        }
        try streamy.descend("three") { sub in
            try sub.append(key: "key", value: "value")
        }
        try streamy.finish()

        var data = Data(count: 1024)
        var bytesRead = 0
        try data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) throws in
            bytesRead = try readEnd.read(into: bytes)
        }
        data.count = bytesRead

        let roundTrip = try #require(try JSONSerialization.jsonObject(with: data, options: .allowFragments) as? [String: Any], "encoded JSON should coerce to [String:Any]")
        #expect(roundTrip.count == 3, "round trip should result in expected number of dict key/value pairs")
        #expect(roundTrip["one"] as? String == "top", "round trip should have correct 'one' value")
        #expect(roundTrip["two"] as? [String] == ["array"], "round trip should have correct 'two' value")
        #expect(roundTrip["three"] as? [String: String] == ["key": "value"], "round trip should have correct 'three' value")
    }

    @Test
    func doubleFinish() async throws {
        let (readEnd, writeEnd) = try FileDescriptor.pipe()
        defer {
            try? readEnd.close()
            try? writeEnd.close()
        }
        let streamy = try StreamingEncoderArray(writeEnd)
        try streamy.finish()
        #expect(throws: StreamingEncoderError.doubleFinish, "finishing twice should throw") { try streamy.finish() }
    }
}
