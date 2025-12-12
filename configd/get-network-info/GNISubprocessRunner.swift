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
	private let logger: Logger = Logger(subsystem: "com.apple.get-network-info",
						   category: "GNISubprocessRunner")
	private let dateFormatter: DateFormatter = {
		let formatter = DateFormatter()
		formatter.dateFormat = "yyyy-MM-dd HH:mm:ss.SSSSSS"
		return formatter
	}()

	final private class GNIOutputTargetFile: TextOutputStream {
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
				print("failed creating output file '\(self.filePath)' with error '\(error)'")
				exit(1)
			}
			guard let fileHandle = FileHandle(forWritingAtPath: filePath.string) else {
				print("failed creating file handle for \(self.filePath)")
				exit(1)
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
				print("failed writing to '\(self.filePath)' with error '\(error)'")
				exit(1)
			}
		}

		func readAll() -> String {
			do {
				return try String(contentsOf: URL(filePath: filePath.description), encoding: .utf8)
			} catch {
				print("failed reading '\(self.filePath)' with error '\(error)'")
				exit(1)
			}
		}

		func close() {
			if !closed {
				do {
					try fileDescriptor.close()
					try fileHandle.close()
				} catch {
					print("failed closing '\(self.filePath)' with error '\(error)'")
					exit(1)
				}
				closed = true
			}
		}
	}

	private var pathRoot: FilePath
	private let runnerTmpStdoutFilename: FilePath = "tmp-gni-output.XXXXXX"
	private let runnerStdoutFilename: FilePath = "get-network-info.txt"
	private var runnerStdoutTarget: GNIOutputTargetFile

	init?(pathRoot: FilePath) {
		self.pathRoot = pathRoot
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
		self.dateFormatter.string(from: Date())
	}

	public func log(_ message: String, at level: OSLogType = .default) {
		print("\(currentTimeString): \(message)", to: &runnerStdoutTarget)
		self.logger.log(level: level, "\(message)")
	}

	public func errorlog(_ message: String) {
		log(message, at: .error)
	}

	// MARK: - Helper Methods

	private func prepareOutputPaths(stdout: String?, stderr: String?) -> (stdoutPath: FilePath, stderrPath: FilePath)? {
		let stdoutPath: FilePath
		if stdout == "/dev/null" {
			stdoutPath = FilePath("/dev/null")
		} else if let stdout = stdout {
			stdoutPath = pathRoot.appending(stdout)
		} else {
			let stdoutURL = FileManager.default.temporaryDirectory.appending(component: runnerTmpStdoutFilename.string)
			guard let path = FilePath(stdoutURL) else {
				errorlog("failed to create temporary file path at url '\(stdoutURL)'")
				return nil
			}
			stdoutPath = path
		}

		let stderrPath: FilePath
		if let stderr = stderr {
			stderrPath = stderr == "/dev/null" ? FilePath("/dev/null") : pathRoot.appending(stderr)
		} else {
			stderrPath = stdoutPath
		}

		return (stdoutPath, stderrPath)
	}

	private func spawnProcess(binaryPath: String, argv: [UnsafeMutablePointer<CChar>?], stdoutPath: FilePath? = nil, stderrPath: FilePath? = nil) -> Int32 {
		var fileActions: posix_spawn_file_actions_t? = nil
		var err: Int32 = 0

		// Only set up file redirection if paths are provided
		if let stdoutPath = stdoutPath, let stderrPath = stderrPath {
			err = posix_spawn_file_actions_init(&fileActions)
			guard err == 0 else {
				errorlog("posix_spawn_file_actions_init failed with error: \(err)")
				return err
			}

			err = posix_spawn_file_actions_addopen(&fileActions, STDOUT_FILENO, stdoutPath.description, O_RDWR | O_APPEND, 0o644)
			guard err == 0 else {
				errorlog("posix_spawn_file_actions_addopen '\(STDOUT_FILENO)' failed with error '\(err)'")
				return err
			}

			err = posix_spawn_file_actions_addopen(&fileActions, STDERR_FILENO, stderrPath.description, O_RDWR | O_APPEND, 0o644)
			guard err == 0 else {
				errorlog("posix_spawn_file_actions_addopen '\(STDERR_FILENO)' failed with error '\(err)'")
				return err
			}
		}

		defer {
			if fileActions != nil {
				posix_spawn_file_actions_destroy(&fileActions)
			}
		}

		var pid: pid_t = -1
		err = posix_spawn(&pid, argv[0], &fileActions, nil, argv, environ)
		guard err == 0 else {
			errorlog("posix_spawn failed with error '\(err)'")
			return err
		}

		let _ = waitpid(pid, &err, 0)
		guard err == 0 else {
			errorlog("waitpid failed with error '\(err)'")
			return err
		}

		return err
	}

	// MARK: - Public Methods

	public func run(command: String, stdout: String? = nil, stderr: String? = nil) -> (Bool, String?) {
		let arguments: [String] = command.components(separatedBy: .whitespacesAndNewlines).filter { !$0.isEmpty }
		if arguments.isEmpty {
			errorlog("gni received empty command")
			return (false, nil)
		}

		guard FileManager.default.fileExists(atPath: arguments[0]) else {
			errorlog("command '\(arguments[0])' doesn't exist")
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

		let skipHeader = arguments[0].hasSuffix("tar") || (stdout?.hasSuffix(".tar") ?? false) || (stdout?.hasSuffix(".plist") ?? false)

		log(command)

		guard let paths = prepareOutputPaths(stdout: stdout, stderr: stderr) else {
			return (false, nil)
		}

		guard let outputFile = GNIOutputTargetFile(filePath: paths.stdoutPath) else {
			errorlog("failed to create output file '\(paths.stdoutPath)'")
			return (false, nil)
		}

		if stdout != nil && !skipHeader {
			outputFile.write("#\n# \(command)\n#\n")
		}

		let err = spawnProcess(binaryPath: arguments[0], argv: argv, stdoutPath: paths.stdoutPath, stderrPath: paths.stderrPath)

		var retstr: String? = nil
		if stdout != nil {
			outputFile.close()
		} else {
			retstr = outputFile.readAll()
			outputFile.close()
			try? FileManager.default.removeItem(atPath: paths.stdoutPath.description)
		}

		return (err == 0, retstr)
	}

	public func runWithArgs(binaryPath: String, arguments: [String], stdout: String? = nil, stderr: String? = nil) -> (Bool, String?) {
		guard FileManager.default.fileExists(atPath: binaryPath) else {
			errorlog("command '\(binaryPath)' doesn't exist")
			return (false, nil)
		}

		var argv: [UnsafeMutablePointer<CChar>?] = [binaryPath].map { $0.withCString { strdup($0) } }
		argv += arguments.map { $0.withCString { strdup($0) } }
		guard argv.count == (arguments.count + 1) else {
			errorlog("failed to create array of c strings for '\(binaryPath)' with \(arguments.count) arguments")
			return (false, nil)
		}
		argv += [nil]
		defer {
			argv.forEach { free($0) }
		}

		let skipHeader = binaryPath.hasSuffix("tar") || (stdout?.hasSuffix(".tar") ?? false) || (stdout?.hasSuffix(".plist") ?? false)
		// Format arguments for display, quoting empty strings
		let displayArgs = arguments.map { $0.isEmpty ? "\"\"" : $0 }
		let commandStr = ([binaryPath] + displayArgs).joined(separator: " ")

		log(commandStr)

		guard let paths = prepareOutputPaths(stdout: stdout, stderr: stderr) else {
			return (false, nil)
		}

		guard let outputFile = GNIOutputTargetFile(filePath: paths.stdoutPath) else {
			errorlog("failed to create output file '\(paths.stdoutPath)'")
			return (false, nil)
		}

		if stdout != nil && !skipHeader {
			outputFile.write("#\n# \(commandStr)\n#\n")
		}

		let err = spawnProcess(binaryPath: binaryPath, argv: argv, stdoutPath: paths.stdoutPath, stderrPath: paths.stderrPath)

		var retstr: String? = nil
		if stdout != nil {
			outputFile.close()
		} else {
			retstr = outputFile.readAll()
			outputFile.close()
			try? FileManager.default.removeItem(atPath: paths.stdoutPath.description)
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

		let err = spawnProcess(binaryPath: arguments[0], argv: argv)
		return err == 0
	}

}

