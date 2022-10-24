//
//  MBCSharePlayMessages.swift
//  MBCSharePlayMessages
//

import Foundation
import os.log

@objc(StartSelectionMessage)
public class StartSelectionMessage: NSObject, Codable {
    @objc public var square: CUnsignedChar
}

@objc(EndSelectionMessage)
public class EndSelectionMessage: NSObject, Codable {
    @objc public var square: CUnsignedChar
    @objc public var animate: Bool
}

@objc(SharePlaySettingsMessage)
public class SharePlaySettingsMessage: NSObject, Codable {
    @objc public var isPlayer: Bool
    @objc public var disconnecting: Bool
}

@objc(SharePlayBoardStateMessage)
public class SharePlayBoardStateMessage: NSObject, Codable {
    @objc public var fen: String
    @objc public var holding: String
    @objc public var moves: String
    @objc public var numMoves: Int32

    @objc public init(fen: String, holding: String, moves:String, numMoves:Int32) {
        self.fen = fen
        self.holding = holding
        self.moves = moves
        self.numMoves = numMoves
    }
}

enum MessageType: Int, Codable {
    case takeBack
}

struct GenericMessage: Codable {
    let type: MessageType
}

struct BoardMessage: Codable {
    let fen: String
    let holding: String
    let moves: String
    let numMoves: Int32
}
