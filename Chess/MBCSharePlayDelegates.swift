//
//  MBCSharePlayDelegates.swift
//  MBCSharePlayDelegates
//

import Foundation

@objc(MBCSharePlayManagerBoardWindowDelegate)
public protocol MBCSharePlayManagerBoardWindowDelegate: AnyObject {
    func connectedToSharePlaySession(numParticipants: Int)
    func receivedStartSelectionMessage(message: StartSelectionMessage)
    func receivedEndSelectionMessage(message: EndSelectionMessage)
    func sendNotificationForGameEnded()
    func createBoardStateMessage() -> SharePlayBoardStateMessage
    func receivedBoardStateMessage(fen: String, moves: String, holding: String)
    func sessionDidEnd()
    func receivedTakeBackMessage()
}

@objc(MBCSharePlayConnectionDelegate)
public protocol MBCSharePlayConnectionDelegate: AnyObject {
    func sharePlayConnectionEstablished()
    func sharePlayDidDisconnectSession(message: SharePlaySettingsMessage)
}
