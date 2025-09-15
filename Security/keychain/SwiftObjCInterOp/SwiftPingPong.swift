//
//  SwiftPingPong.swift
//

import Foundation

@objcMembers
class PingPong: NSObject {
    var ping: String
    var pong: String
    // Default initializer with default values for ping and pong
    override init() {
        self.ping = "ping"
        self.pong = "pong"
        super.init()
    }

    // Initializer to set initial values for ping and pong
    init(ping: String, pong: String) {
        self.ping = ping
        self.pong = pong
        super.init()
    }

    // Method to print the values of ping and pong
    func printPingPong() {
        print("Ping: \(ping), Pong: \(pong)")
    }

    // Method to return value of the ping
    func pingValue() -> String {
        return self.ping
    }

    // Method to return value of the pong
    func pongValue() -> String {
        return self.pong
    }
}
