//
//  SFAnalyticsUploadTests.swift
//


import XCTest
import Compression

class SFAnalyticsUploadTests: SupdTests {
    var uuid: UUID!
    
    override func setUp() {
        super.setUp()
        self.uuid = UUID()
    }
    
    func testPath() -> URL {
        FileManager.default.temporaryDirectory.appending(path: self.uuid.uuidString)
    }
    
    func fillDatabaseWithLargeStuffs() {
        // 52488 bytes
        var longString = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        for _ in 1 ..< 7 {
            longString += longString + longString;
        }
        
        let attrs: [String: Any] = [
            "attr": longString
        ]
        
        // more then 1000 so we rotate logs
        for _ in 1 ..< 1010 {
            autoreleasepool {
                self.swtransparencyAnalytics.logHardFailure(forEventNamed: "fail", withAttributes: attrs)
            }
        }
    }
    
    func testSFAnalyticsTopicGenerator() async throws {
        let fileManager = FileManager.default

        guard let topic = swTransparencyTopic() else {
            XCTFail("transparencyTopic failed")
            return
        }
        guard let client = topic.topicClients.first else {
            XCTFail("client failed")
            return
        }
        XCTAssertEqual(client.name, "swtransparency")
        
        let testPath = self.testPath()
        try fileManager.createDirectory(at: testPath, withIntermediateDirectories: true)
        
        let generator = SFAnalyticsTopicGenerator(topic: topic)
        generator.deleteAllUploadFiles(topicClient: client, outputDirectory: testPath)
        
        guard let c1 = try? fileManager.contentsOfDirectory(at: testPath, includingPropertiesForKeys: []) else {
            XCTFail("contentsOfDirectory")
            return
        }
        XCTAssertEqual(c1.count, 0)
        
        XCTAssertEqual(generator.uploadFiles(topicClient: client), [])

        self.fillDatabaseWithLargeStuffs()
        
        try generator.generate(topicClient: client,
                               outputDirectory: testPath,
                               uploadSizeLimit: 2_000_000,
                               eventQuota: 1_000,
                               uuid: self.uuid)
        
        // check that there are expected output file
        guard let c2 = try? fileManager.contentsOfDirectory(at: self.testPath(), includingPropertiesForKeys: []) else {
            XCTFail("contentsOfDirectory")
            return
        }
        
        let expectedFileCount = 27
        
        XCTAssertEqual(c2.count, expectedFileCount)

        let uploadFiles = generator.uploadFiles(topicClient: client)
        XCTAssertEqual(uploadFiles.count, expectedFileCount)
        
        // parse each upload file to make sure is JSON-esk
        try uploadFiles.forEach { file in
            let data = try Data(contentsOf: file)
            do {
                let j = try JSONSerialization.jsonObject(with: data)  as? NSDictionary
                XCTAssertNotNil(j)
                XCTAssertNotNil(j?["postTime"], "should have posttime")
                XCTAssertNotNil(j?["events"], "should have events")
                let events = j?["events"] as! [NSDictionary]
                XCTAssert(events.count > 1, "should have more then one member in events array")
            } catch {
                XCTFail("JSONSerialization.jsonObject \(error)")
                return
            }
        }
        
        let numberOfFilesToUpload = 5
        
        XCTAssert(uploadFiles.count > numberOfFilesToUpload, "should have at least \(numberOfFilesToUpload) files")
        
        // "upload" first five
        if uploadFiles.count > numberOfFilesToUpload {
            for uploadFile in uploadFiles[0..<numberOfFilesToUpload] {
                generator.confirmUploadFile(topicClient: client, url: uploadFile)
            }
        } else {
            XCTFail("not enough files to upload")
        }
        
        let uploadFilesPastDelete = generator.uploadFiles(topicClient: client)
        XCTAssertEqual(uploadFilesPastDelete.count, expectedFileCount - numberOfFilesToUpload)

        // delete them all
        generator.deleteAllUploadFiles(topicClient: client, outputDirectory: testPath)

        // see that they are deleted
        guard let c3 = try? fileManager.contentsOfDirectory(at: testPath, includingPropertiesForKeys: []) else {
            XCTFail("contentsOfDirectory")
            return
        }
        XCTAssertEqual(c3.count, 0)


        let uploadFiles3 = generator.uploadFiles(topicClient: client)
        XCTAssertEqual(uploadFiles3.count, 0)

    }
    
    func testSFAClassicJSON() async throws {
        guard let topic = swTransparencyTopic() else {
            XCTFail("transparencyTopic failed")
            return
        }
        guard let client = topic.topicClients.first else {
            XCTFail("client failed")
            return
        }
        XCTAssertEqual(client.name, "swtransparency")

        self.fillDatabaseWithLargeStuffs()
        
        let data = try await self.supd.createLoggingJSON(false, topic: topic.internalTopicName)
        print("data size: \(data?.count ?? -1)")
    }
}
