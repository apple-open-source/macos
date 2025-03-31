/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

import Foundation
import OSLog


private let SFAUploadFileTable = "upload_file"

@objc class SFAnalyticsTopicGenerator: NSObject {
    enum SFAnalyticsTopicGeneratorError: CustomNSError {
        static let errorDomain: String = "com.apple.securityuploadd.SFAnalyticsTopicGeneratorError"
        case fileHandleNotOpen
    }
    let topic: SFAnalyticsTopic
    let logger: Logger
    let uploadTime: UInt64
    
    @objc init(topic: SFAnalyticsTopic) {
        self.topic = topic
        self.logger = Logger(subsystem: "SFAnalyticsTopicGenerator", category: "")
        self.uploadTime = UInt64(Date().timeIntervalSince1970 * 1000)
    }
    
    /*
     * Build output files with JSON content (array of element), assume input
     * is JSON from the database, so just concat the data with JSON goop
     * for less memory use and speed, this so that file can be passed directly
     * to the Networking api to send directly to server.
     */
    class OutputFile {
        let store: SFAnalyticsSQLiteStore
        let topic: SFAnalyticsTopic
        let outputDirectory: URL
        let limit: off_t
        var currentSize: off_t
        var currentFile: URL?
        var fileCount: Int = 0
        var fileHandle: FileHandle?
        var filterError: Error? = nil
        var firstElement: Bool
        let logger: Logger
        let uploadTime: UInt64
        let linkedID: String

        init(topic: SFAnalyticsTopic, store: SFAnalyticsSQLiteStore, outputDirectory: URL, limit: off_t, uploadTime: UInt64, linkedID: String, logger: Logger) throws {
            self.store = store
            self.topic = topic
            self.outputDirectory = outputDirectory
            self.limit = limit
            self.currentSize = 0
            self.currentFile = nil
            self.firstElement = true
            self.logger = logger
            self.uploadTime = uploadTime
            self.linkedID = linkedID

            try createFileIfNeeded()
        }

        func writeToFileHandle(data: Data?) throws {
            guard let data else {
                return
            }
            guard let fileHandle = self.fileHandle else {
                throw SFAnalyticsTopicGeneratorError.fileHandleNotOpen
            }
            try fileHandle.write(contentsOf: data)
        }
        
        static func stringName(topic: SFAnalyticsTopic, store: SFAnalyticsSQLiteStore, fileCount: Int) -> String {
            "OutputFile-\(topic.internalTopicName)-\(store.databaseBasename)-\(fileCount).json"
        }
        
        // Create file if needed
        func createFileIfNeeded() throws {
            guard self.fileHandle == nil else {
                return
            }
            let filename = Self.stringName(topic: topic, store: store, fileCount: fileCount)
            let file = outputDirectory.appending(path: filename)
            self.currentFile = file
            
            FileManager.default.createFile(atPath: file.path, contents: nil)
            self.fileHandle = try FileHandle(forWritingTo: file)
            
            self.currentSize = 0
            self.firstElement = true
            
            try self.insert(string: "{\"postTime\":\(uploadTime),\"events\":[")
        }

        // Insert file and rotate to a new file if needed
        func insert(event: Data) throws {
            
            // recode with required/denied fields rules applies
            guard let event = self.topic.applyFilterLogic(event, linkedID: linkedID) else {
                self.logger.info("object in \(self.linkedID) was not allowed/valid")
                return
            }
            
            if (self.currentSize + (Int64)(event.count) > self.limit) {
                try rotate()
                try createFileIfNeeded()
            }
            if firstElement == false {
                try self.insert(string: ",")
                self.currentSize += 1
            } else {
                firstElement = false
            }
            self.currentSize += off_t(event.count)
            do {
                try self.writeToFileHandle(data: event)
            } catch {
                self.filterError = error
                throw error
            }
        }
                
        func rotate() throws {
            try commit()
            
            fileCount += 1
            self.currentSize = 0
            self.firstElement = true
            self.fileHandle = nil
            self.currentFile = nil
        }
        
        func insert(string: String) throws {
            guard fileHandle != nil else {
                return
            }
            guard let data = string.data(using: .utf8) else {
                return
            }
            self.currentSize += off_t(data.count)
            do {
                try self.writeToFileHandle(data: data)
            } catch {
                self.filterError = error
                throw error
            }
        }

        func commit() throws {
            if let fileHandle {
                try self.insert(string: "]}")
                try fileHandle.close()
            }

            // did we have write errors in the filter
            if let filterError = self.filterError {
                self.filterError = nil
                throw filterError
            }

            if let currentFile {
                self.logger.info("commiting log file \(currentFile.path)")

                store.insertOrReplace(into: SFAUploadFileTable, values: [
                    "file": "\(currentFile.path)",
                    "store": "\(store.databaseBasename)",
                    "timestamp": Date().timeIntervalSince1970 * 1000,
                ])
            }
            self.fileHandle = nil
            self.currentFile = nil
        }
    }
    
    @objc func uploadFiles(topicClient: SFAnalyticsClient) -> [URL] {
        var urls: [URL] = []
        topicClient.withStore { store in
            store.select(from: SFAUploadFileTable, 
                         where: "store = ?",
                         bindings: [store.databaseBasename],
                         orderBy: nil,
                         limit: nil)
            { item, stop in
                guard let dict = item as? Dictionary<String,AnyObject> else {
                    return
                }
                guard let file = dict["file"] as? String else {
                    return
                }
                let url = URL(fileURLWithPath: file)
                urls.append(url)
            }
        }
        return urls
    }

    @objc func confirmUploadFile(topicClient: SFAnalyticsClient, url: URL) {
        topicClient.withStore { store in
            /* remove url from store */
            store.delete(from: SFAUploadFileTable, matchingValues: [
                "store" : store.databaseBasename,
                "file": url.path,
            ])
            try? FileManager.default.removeItem(at: url)
        }
    }
    
    func deleteUploadFiles(store: SFAnalyticsSQLiteStore, outputDirectory: URL) {
        // First delete all files that are in the output directory
        if let items = store.select(["file"], from: SFAUploadFileTable, where: "store = ?", bindings: [store.databaseBasename]) {
            for item in items {
                guard let file = item["file"] as? String else {
                    continue
                }
                guard file.hasPrefix(outputDirectory.path) else {
                    continue
                }
                try? FileManager.default.removeItem(atPath: file)
            }
        }
        store.delete(from: SFAUploadFileTable, matchingValues: ["store" : store.databaseBasename])
    }
    
    @objc func deleteAllUploadFiles(topicClient: SFAnalyticsClient, outputDirectory: URL) {
        topicClient.withStore { store in
            deleteUploadFiles(store: store, outputDirectory: outputDirectory)
        }
    }

    @objc func generate(topicClient: SFAnalyticsClient,
                        outputDirectory: URL,
                        uploadSizeLimit: off_t,
                        eventQuota: UInt,
                        uuid: UUID) throws
    {
        topicClient.withStore { store in
            
            deleteUploadFiles(store: store, outputDirectory: outputDirectory)

            let outputFile: OutputFile
            
            do {
                outputFile = try OutputFile(topic: topic,
                                            store: store,
                                            outputDirectory: outputDirectory,
                                            limit: uploadSizeLimit,
                                            uploadTime: uploadTime,
                                            linkedID: uuid.uuidString,
                                            logger: logger)
            } catch {
                logger.log("failed to create output file generator \(error)")
                return
            }
                    
            var numEvents: UInt = 0
            
            // Always include health event
            if let health = topic.healthSummary(withName: topicClient,
                                                store: store,
                                                uuid: uuid,
                                                timestamp: nil,
                                                lastUploadTime: nil) {
                if let appleStatus = topic.appleInternalStatus() {
                    health.addEntries(from: appleStatus)
                }
                if let h = try? JSONSerialization.data(withJSONObject: health, options: []) {
                    try? outputFile.insert(event: h)
                    numEvents += 1
                }
            }
            store.streamEvents(withLimit: nil, fromTable: SFAnalyticsTableRockwell) { row in
                try? outputFile.insert(event: row as Data)
                numEvents += 1
                return (eventQuota > numEvents);
            }
            store.streamEvents(withLimit: nil, fromTable: SFAnalyticsTableHardFailures) { row in
                try? outputFile.insert(event: row as Data)
                numEvents += 1
                return (eventQuota > numEvents);
            }
            store.streamEvents(withLimit: nil, fromTable: SFAnalyticsTableSoftFailures) { row in
                try? outputFile.insert(event: row as Data)
                numEvents += 1
                return (eventQuota > numEvents);
            }
            
            try? outputFile.commit()
            
            store.clearAllData()
        }
    }
}
