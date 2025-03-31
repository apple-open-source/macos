/*
 * Copyright (c) 2024-2025 Apple Inc. All rights reserved.
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
import os
import System
import Darwin

final class GNISubprocessRunner {
	static let logger: Logger = Logger(subsystem: "com.apple.get-network-info",
					   category: "GNISubprocessRunner")
	static let dateFormatter: DateFormatter = {
		let formatter = DateFormatter()
		formatter.dateFormat = "yyyy-MM-dd HH:mm:ss.SSSSSS"
		return formatter
	}()

	final private  class GNIOutputTargetFile: TextOutputStream {
		private let filePath: FilePath
		private let fileDescriptor: FileDescriptor
		private let fileHandle: FileHandle
		private var closed: Bool = false

		init?(filePath: FilePath) {
			self.filePath = filePath
			do {
				self.fileDescriptor = try FileDescriptor.open(self.filePath,
									      // O_RDWR
									      .readWrite,
									      // O_CREAT | O_APPEND
									      options: [.create, .append],
									      // 0o644
									      permissions: [.ownerReadWrite, .groupRead, .otherRead])
			} catch {
				fatalError("failed creating output file '\(self.filePath)' with error '\(error)'")
				return nil
			}
			guard let fileHandle = FileHandle(forWritingAtPath: filePath.string) else {
				fatalError("failed creating file handle for \(self.filePath)")
				return nil
			}
			self.fileHandle = fileHandle
		}

		deinit {
			self.close()
		}

		func write(_ string: String) {
			do {
				fileHandle.seekToEndOfFile()
				try fileHandle.write(contentsOf: string.data(using: .utf8)!)
			} catch {
				fatalError("failed writing to '\(self.filePath)' with error '\(error)'")
			}
		}

		func readAll() -> String {
			do {
				return try String(contentsOf: URL(filePath: filePath.description), encoding: .utf8)
			} catch {
				fatalError("failed reading '\(self.filePath)' with error '\(error)'")
			}
		}

		func close() {
			if !closed {
				do {
					try fileDescriptor.close()
					try fileHandle.close()
				} catch {
					fatalError("failed closing '\(self.filePath)' with error '\(error)'")
				}
				closed = true
			}
		}
	}

	private var pathRoot: FilePath = "/tmp"
	private let runnerTmpStdoutFilename: FilePath = "tmp-gni-output.txt"
	private let runnerStdoutFilename: FilePath = "get-network-info.txt"
	private var runnerStdoutTarget: GNIOutputTargetFile

	init?(pathRoot: FilePath? = nil) {
		self.pathRoot = pathRoot ?? self.pathRoot
		let runnerOutputFilePath = self.pathRoot.appending("\(self.runnerStdoutFilename)")
		guard let runnerStdoutTarget = GNIOutputTargetFile(filePath: runnerOutputFilePath) else {
			return nil
		}
		self.runnerStdoutTarget = runnerStdoutTarget
	}

	deinit {
		self.runnerStdoutTarget.close()
	}

	var currentTimeString: String {
		GNISubprocessRunner.dateFormatter.string(from: Date())
	}

	public func log(_ message: String, at level: OSLogType = .default) {
		print("\(currentTimeString): \(message)", to: &runnerStdoutTarget)
		GNISubprocessRunner.logger.log(level: level, "\(message)")
	}

	public func errorlog(_ message: String) {
		log(message, at: .error)
	}

	public func run(command: String, stdout: String? = nil, stderr: String? = nil) -> (Bool, String?) {
		var err: Int32 = 0
		let arguments: [String] = command.components(separatedBy: .whitespacesAndNewlines).filter { !$0.isEmpty }
		if (arguments.isEmpty) {
			errorlog("gni received empty command")
			return (false, nil)
		}
		var argv: [UnsafeMutablePointer<CChar>?] = arguments.map { $0.withCString { strdup($0) } }
		guard argv.count == arguments.count else {
			errorlog("failed to create array of c strings from given command '\(command)'")
			return (false, nil)
		}
		argv += [nil]
		defer {
			argv.forEach { free($0) }
		}
		var skip_header: Bool = false

		if arguments[0].hasSuffix("tar") || (stdout?.hasSuffix(".tar") ?? false) || (stdout?.hasSuffix(".plist") ?? false) {
			skip_header = true
		}
		guard FileManager.default.fileExists(atPath: arguments[0]) else {
			errorlog("command '\(arguments[0])' doesn't exist")
			return (false, nil)
		}

		// parent writes into its log file
		log("\(command)")

		// this redirects stdout and stderr accordingly
		let stdoutPath: FilePath
		let stderrPath: FilePath
		if stdout == "/dev/null" {
			stdoutPath = FilePath("/dev/null")
		} else if stdout != nil {
			stdoutPath = pathRoot.appending(stdout!)
		} else {
			stdoutPath = pathRoot.appending(runnerTmpStdoutFilename.string)
		}
		if stderr == nil {
			stderrPath = stdoutPath
		} else if stderr == "/dev/null" {
			stderrPath = FilePath("/dev/null")
		} else {
			stderrPath = pathRoot.appending(stderr!)
		}

		guard let outputFile: GNIOutputTargetFile = GNIOutputTargetFile(filePath: stdoutPath) else {
			errorlog("failed to create output file '\(stdoutPath)'")
			return (false, nil)
		}

		if stdout != nil && !skip_header {
			// parent writes into subprocess output file
			outputFile.write("#\n")
			outputFile.write("# \(command)\n")
			outputFile.write("#\n")
		}

		var fileActions: posix_spawn_file_actions_t? = nil
		err = posix_spawn_file_actions_init(&fileActions)
		defer {
			posix_spawn_file_actions_destroy(&fileActions);
		}
		guard err == 0 else {
			errorlog("posix_spawn_file_actions_init failed with error: \(err)")
			return  (false, nil)
		}

		// child writes into subprocess output file
		err = posix_spawn_file_actions_addopen(&fileActions,
						       STDOUT_FILENO,
						       stdoutPath.description,
						       O_RDWR | O_APPEND,
						       0o644)
		guard err == 0 else {
			errorlog("posix_spawn_file_actions_addopen '\(STDOUT_FILENO)' failed with error '\(err)'")
			return  (false, nil)
		}

		// both stdout and stderr are redirected to the same output file
		err = posix_spawn_file_actions_addopen(&fileActions,
						       STDERR_FILENO,
						       stderrPath.description,
						       O_RDWR | O_APPEND,
						       0o644)
		guard err == 0 else {
			errorlog("posix_spawn_file_actions_addopen '\(STDERR_FILENO)' failed with error '\(err)'")
			return (false, nil)
		}

		var pid: pid_t = -1
		err = posix_spawn(&pid, argv[0], &fileActions, nil, argv, environ)
		guard err == 0 else {
			errorlog("posix_spawn failed with error '\(err)'")
			return (false, nil)
		}

		let _ = waitpid(pid, &err, 0)
		guard err == 0 else {
			errorlog("waitpid failed with error '\(err)'")
			return (false, nil)
		}

		var retstr: String? = nil
		if stdout != nil {
			outputFile.close()
		} else {
			// use this to deal with any temp files created for info we don't persist, like iflist
			retstr = outputFile.readAll()
			outputFile.close()
			try? FileManager.default.removeItem(atPath: stdoutPath.description)
		}

		return (err == 0, retstr)
	}

	public func runClassic(command: String) -> Bool {
		let arguments: [String] = command.components(separatedBy: .whitespacesAndNewlines).filter { !$0.isEmpty }
		var argv: [UnsafeMutablePointer<CChar>?] = arguments.map { $0.withCString { strdup($0) } }
		argv += [nil]
		defer {
			argv.forEach { free($0) }
		}

		var pid: pid_t = -1
		var err = posix_spawn(&pid, argv[0], nil, nil, argv, environ)
		guard err == 0 else {
			errorlog("posix_spawn failed with error '\(err)'")
			return false
		}

		let _ = waitpid(pid, &err, 0)
		guard err == 0 else {
			errorlog("waitpid failed with error '\(err)'")
			return false
		}

		return err == 0
	}

}

