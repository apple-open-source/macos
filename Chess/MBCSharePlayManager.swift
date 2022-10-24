//
//  MBCSharePlayManager.swift
//  MBCSharePlayManager
//

import Foundation
import os.log
import GroupActivities
import AppKit
import Combine

@objc(MBCSharePlayManager)
public class MBCSharePlayManager : NSObject {
    
    private static var sharedManager: MBCSharePlayManager = {
        let manager = MBCSharePlayManager()
        return manager
    }()
    
    @objc public weak var boardWindowDelegate: MBCSharePlayManagerBoardWindowDelegate?
    @objc public weak var connectionDelegate: MBCSharePlayConnectionDelegate?
    @objc public var connected: Bool
    @objc public var totalMoves: Int
    var groupSession: GroupSession<ChessTogether>?
    var messenger: GroupSessionMessenger?
    var groupStateObserver = GroupStateObserver()
    var subscriptions = Set<AnyCancellable>()
    var tasks = Set<Task<Void, Never>>()
    
    @objc class func sharedInstance() -> MBCSharePlayManager {
        return sharedManager
    }
    
    @objc public override init() {
        connected = false
        totalMoves = 0
    }
    
    @objc public func leaveSession() {
        os_log("MBCSharePlayManager: Leaving Shareplay Session")
        groupSession?.leave()
    }
    
    @objc public func startSharing() async {
        if groupStateObserver.isEligibleForGroupSession {
            do {
                try await ChessTogether().activate()
            } catch {
                os_log("MBCSharePlayManager: Chess Group Session Activiation Failed")
            }
        } else {
            os_log("MBCSharePlayManager startSharing() couldnt activate groupSession=%d and isEligibleForGroupSession=%d", groupSession == nil ? 0 : 1, groupStateObserver.isEligibleForGroupSession)
        }
    }
    
    @objc public func configureSharePlaySession() async {
        for await newSession in ChessTogether.sessions() {
            //when a new shareplay becomes available we want to allow new connection to take over
            os_log("MBCSharePlayManager: configureSharePlaySession found a new session to join")
            connected = false;
            connectionDelegate?.sharePlayConnectionEstablished()
            await configureGroupSession(newSession)
        }
    }
    
    func configureGroupSession(_ groupSession: GroupSession<ChessTogether>) async {
        self.groupSession = groupSession
        let messenger = GroupSessionMessenger(session: groupSession)
        self.messenger = messenger
        
        guard self.groupSession != nil && self.messenger != nil else {
            os_log("MBCSharePlayManager: GroupSession or GroupSessionMessenger is nil")
            return;
        }
        
        groupSession.$state
            .sink { state in
                if case .invalidated = state {  //A state that indicates the session is no longer valid and can't be used for shared activities.
                    os_log("MBCSharePlayManager: State Changed, Leaving Shareplay Session")
                    self.groupSession = nil
                    self.tasks.forEach { $0.cancel() }
                    self.tasks = []
                    self.connected = false
                    self.subscriptions = []
                    self.messenger = nil
                    self.totalMoves = 0;
                    self.boardWindowDelegate?.sessionDidEnd()
                }
            }
            .store(in: &subscriptions)
        
        //adding when new person joins
        groupSession.$activeParticipants
            .sink { activeParticipants in
                let newParticipants = activeParticipants.subtracting(groupSession.activeParticipants)
                if self.totalMoves > 0 {
                    async {
                        let boardStateMessage = self.boardWindowDelegate?.createBoardStateMessage()
                        os_log("MBCSharePlayManager: sending BoardStateMessage")
                        if boardStateMessage != nil {
                            do {
                                try await messenger.send(BoardMessage(fen: boardStateMessage?.fen ?? "", holding: boardStateMessage?.holding ?? "", moves: boardStateMessage?.moves ?? "", numMoves: boardStateMessage?.numMoves ?? 0), to: .only(newParticipants))
                            } catch {
                                os_log("MBCSharePlayManager: BoardMessage Send Fail")
                            }
                        }
                    }
                } else {
                    os_log("MBCSharePlayManager: dont send update board, fresh board")
                }
            }
            .store(in: &subscriptions)
        
        var task = async {
            for await (message, _) in messenger.messages(of: StartSelectionMessage.self) {
                os_log("MBCSharePlayManager: Received StartSelectionMessage")
                handle(message)
            }
        }
        tasks.insert(task)
        
        task = async {
            for await (message, _) in messenger.messages(of: EndSelectionMessage.self) {
                os_log("MBCSharePlayManager: Received EndSelectionMessage")
                handle(message)
            }
        }
        tasks.insert(task)
        
        task = async {
            for await (message, _) in messenger.messages(of: BoardMessage.self) {
                os_log("MBCSharePlayManager: Received SharePlayBoardStateMessage")
                handle(message)
            }
        }
        tasks.insert(task)
        
        task = async {
            for await (message, _) in messenger.messages(of: GenericMessage.self) {
                os_log("MBCSharePlayManager: Received TakeBackMessage")
                handle(message)
            }
        }
        
        do {
            try await groupSession.join()
        } catch {
            os_log("MBCSharePlayManager: Failed to Join Group Session")
        }
        self.assignPlayerRole()
    }
    
    // MARK: Handles for incoming messages
    
    func handle(_ message: StartSelectionMessage) {
        boardWindowDelegate?.receivedStartSelectionMessage(message: message)
    }
    
    func handle(_ message: EndSelectionMessage) {
        boardWindowDelegate?.receivedEndSelectionMessage(message: message)
    }
        
    func handle(_ message: BoardMessage) {
        boardWindowDelegate?.receivedBoardStateMessage(fen: message.fen, moves: message.moves, holding: message.holding)
    }
    
    func handle(_ message: GenericMessage) {
        if message.type == MessageType.takeBack {
            totalMoves = totalMoves - 2; //When we take back moves, we take back the last 2 that were played
            boardWindowDelegate?.receivedTakeBackMessage()
        } else {
            os_log("MBCSharePlayManager: Generic Message undefined message type")
        }
    }
    
    func displayNotification() {
        print("MBCSharePlayManager: display notification")
        boardWindowDelegate?.sendNotificationForGameEnded()
    }
    
    @objc public func assignPlayerRole() {
        if groupSession != nil {
            let numParticipants = groupSession?.activeParticipants.count
            self.boardWindowDelegate?.connectedToSharePlaySession(numParticipants: numParticipants!)
        } else {
            os_log("MBCSharePlayManager: groupSession nil")
        }
    }
    
    // MARK: Message Sending Functions
    
    @objc public func sendStartSelectionMessage(message: StartSelectionMessage) {
        if let messenger = messenger {
            async {
                do {
                    try await messenger.send(message)
                } catch {
                    os_log("MBCSharePlayManager: StartSelectionMessage Sent Failed")
                }
            }
        }
    }
    
    @objc public func sendEndSelectionMessage(message: EndSelectionMessage) {
        if let messenger = messenger {
            async {
                do {
                    try await messenger.send(message)
                } catch {
                    os_log("MBCSharePlayManager: EndSelectionMessage Sent Failed")
                }
            }
        }
    }
    
    @objc public func sendTakeBackMessage() {
        let message = GenericMessage(type: MessageType.takeBack)
        if let messenger = messenger {
            async {
                do {
                    try await messenger.send(message)
                } catch {
                    os_log("MBCSharePlayManager: TakeBackMesage Sent Failed")
                }
            }
        }
    }
    
}

struct ChessTogether: GroupActivity {
    public var metadata: GroupActivityMetadata {
        var metadata = GroupActivityMetadata()
        metadata.title = NSLocalizedString("Chess", comment: "Play Chess Together")
        metadata.type = .generic
        return metadata
    }

}
