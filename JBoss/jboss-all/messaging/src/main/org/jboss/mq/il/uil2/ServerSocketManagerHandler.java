package org.jboss.mq.il.uil2;

import java.rmi.RemoteException;
import javax.jms.Destination;

import org.jboss.logging.Logger;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.TransactionRequest;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.il.uil2.msgs.BaseMsg;
import org.jboss.mq.il.uil2.msgs.ConnectionTokenMsg;
import org.jboss.mq.il.uil2.msgs.AcknowledgementRequestMsg;
import org.jboss.mq.il.uil2.msgs.MsgTypes;
import org.jboss.mq.il.uil2.msgs.AddMsg;
import org.jboss.mq.il.uil2.msgs.BrowseMsg;
import org.jboss.mq.il.uil2.msgs.CheckIDMsg;
import org.jboss.mq.il.uil2.msgs.CreateDestMsg;
import org.jboss.mq.il.uil2.msgs.DeleteTemporaryDestMsg;
import org.jboss.mq.il.uil2.msgs.GetIDMsg;
import org.jboss.mq.il.uil2.msgs.TemporaryDestMsg;
import org.jboss.mq.il.uil2.msgs.ReceiveMsg;
import org.jboss.mq.il.uil2.msgs.EnableConnectionMsg;
import org.jboss.mq.il.uil2.msgs.SubscribeMsg;
import org.jboss.mq.il.uil2.msgs.TransactMsg;
import org.jboss.mq.il.uil2.msgs.UnsubscribeMsg;
import org.jboss.mq.il.uil2.msgs.DeleteSubscriptionMsg;
import org.jboss.mq.il.uil2.msgs.CheckUserMsg;
import org.jboss.mq.il.uil2.msgs.PingMsg;
import org.jboss.mq.il.Invoker;

/** This is the SocketManager callback handler for the UIL2 server side
 * socket. This handles messages that are requests from clients.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public class ServerSocketManagerHandler implements MsgTypes, SocketManagerHandler
{
   private static Logger log = Logger.getLogger(ServerSocketManagerHandler.class);

   private ConnectionToken connectionToken;
   private Invoker server;
   private SocketManager socketMgr;
   private boolean closed;

   public ServerSocketManagerHandler(Invoker server, SocketManager socketMgr)
   {
      this.server = server;
      this.socketMgr = socketMgr;
      this.closed = false;
   }

   /** The callback from the SocketManager
    * @param msg
    */
   public void handleMsg(BaseMsg msg)
      throws Exception
   {
      boolean trace = log.isTraceEnabled();
      int msgType = msg.getMsgType();
      if (trace)
         log.trace("Begin handleMsg, msgType: " + msgType);

      switch (msgType)
      {
         case m_setSpyDistributedConnection:
            log.debug("Setting up the UILClientIL Connection");
            ConnectionTokenMsg cmsg = (ConnectionTokenMsg) msg;
            connectionToken = cmsg.getToken();
            UILClientIL clientIL = (UILClientIL) connectionToken.clientIL;
            clientIL.setSocketMgr(socketMgr);
            socketMgr.sendReply(msg);
            log.debug("The UILClientIL Connection is set up");
            break;
         case m_acknowledge:
            AcknowledgementRequestMsg ackmsg = (AcknowledgementRequestMsg) msg;
            AcknowledgementRequest ack = ackmsg.getAck();
            server.acknowledge(connectionToken, ack);
            socketMgr.sendReply(msg);
            break;
         case m_addMessage:
            AddMsg amsg = (AddMsg) msg;
            server.addMessage(connectionToken, amsg.getMsg());
            socketMgr.sendReply(msg);
            break;
         case m_browse:
            BrowseMsg bmsg = (BrowseMsg) msg;
            SpyMessage[] msgs = server.browse(connectionToken, bmsg.getDest(), bmsg.getSelector());
            bmsg.setMessages(msgs);
            socketMgr.sendReply(msg);
            break;
         case m_checkID:
            CheckIDMsg idmsg = (CheckIDMsg) msg;
            String ID = idmsg.getID();
            server.checkID(ID);
            if (connectionToken != null)
               connectionToken.setClientID(ID);
            socketMgr.sendReply(msg);
            break;
         case m_connectionClosing:
            server.connectionClosing(connectionToken);
            closed = true;
            socketMgr.sendReply(msg);
            socketMgr.stop();
            break;
         case m_createQueue:
            CreateDestMsg cqmsg = (CreateDestMsg) msg;
            Destination queue = server.createQueue(connectionToken, cqmsg.getName());
            cqmsg.setDest(queue);
            socketMgr.sendReply(msg);
            break;
         case m_createTopic:
            CreateDestMsg ctmsg = (CreateDestMsg) msg;
            Destination topic = server.createTopic(connectionToken, ctmsg.getName());
            ctmsg.setDest(topic);
            socketMgr.sendReply(msg);
            break;
         case m_deleteTemporaryDestination:
            DeleteTemporaryDestMsg dtdmsg = (DeleteTemporaryDestMsg) msg;
            SpyDestination tmpdest = dtdmsg.getDest();
            server.deleteTemporaryDestination(connectionToken, tmpdest);
            socketMgr.sendReply(msg);
            break;
         case m_getID:
            GetIDMsg gidmsg = (GetIDMsg) msg;
            String gid = server.getID();
            if (connectionToken != null)
               connectionToken.setClientID(gid);
            gidmsg.setID(gid);
            socketMgr.sendReply(msg);
            break;
         case m_getTemporaryQueue:
            TemporaryDestMsg tqmsg = (TemporaryDestMsg) msg;
            Destination tmpQueue = server.getTemporaryQueue(connectionToken);
            tqmsg.setDest(tmpQueue);
            socketMgr.sendReply(msg);
            break;
         case m_getTemporaryTopic:
            TemporaryDestMsg ttmsg = (TemporaryDestMsg) msg;
            Destination tmpTopic = server.getTemporaryTopic(connectionToken);
            ttmsg.setDest(tmpTopic);
            socketMgr.sendReply(msg);
            break;
         case m_receive:
            ReceiveMsg rmsg = (ReceiveMsg) msg;
            SpyMessage reply = server.receive(connectionToken, rmsg.getSubscriberID(), rmsg.getWait());
            rmsg.setMessage(reply);
            socketMgr.sendReply(msg);
            break;
         case m_setEnabled:
            EnableConnectionMsg ecmsg = (EnableConnectionMsg) msg;
            server.setEnabled(connectionToken, ecmsg.isEnabled());
            socketMgr.sendReply(msg);
            break;
         case m_subscribe:
            SubscribeMsg smsg = (SubscribeMsg) msg;
            server.subscribe(connectionToken, smsg.getSubscription());
            socketMgr.sendReply(msg);
            break;
         case m_transact:
            TransactMsg tmsg = (TransactMsg) msg;
            TransactionRequest trans = tmsg.getRequest();
            server.transact(connectionToken, trans);
            socketMgr.sendReply(msg);
            break;
         case m_unsubscribe:
            UnsubscribeMsg umsg = (UnsubscribeMsg) msg;
            server.unsubscribe(connectionToken, umsg.getSubscriptionID());
            socketMgr.sendReply(msg);
            break;
         case m_destroySubscription:
            DeleteSubscriptionMsg dsmsg = (DeleteSubscriptionMsg) msg;
            DurableSubscriptionID dsub = dsmsg.getSubscriptionID();
            server.destroySubscription(connectionToken, dsub);
            socketMgr.sendReply(msg);
            break;
         case m_checkUser:
            CheckUserMsg cumsg = (CheckUserMsg) msg;
            String uid = server.checkUser(cumsg.getUsername(), cumsg.getPassword());
            cumsg.setID(uid);
            cumsg.clearPassword();
            socketMgr.sendReply(msg);
            break;
         case m_ping:
            PingMsg ping = (PingMsg) msg;
            server.ping(connectionToken, ping.getTime());
            break;
         case m_pong:
            break;
         case m_authenticate:
            CheckUserMsg cumsg2 = (CheckUserMsg) msg;
            String sessionID = server.authenticate(cumsg2.getUsername(), cumsg2.getPassword());
            cumsg2.setID(sessionID);
            cumsg2.clearPassword();
            socketMgr.sendReply(msg);
            break;
         default:
            throw new RemoteException("Unknown msgType: "+msgType);
      }
      if (trace)
         log.trace("End handleMsg, msgType: " + msgType);
   }

   public void onStreamNotification(Object stream, int size)
   {
   }

   public void asynchFailure(String error, Throwable e)
   {
      log.debug(error, e);
   }

   public void close()
   {
      try
      {
         if (closed == false)
            server.connectionClosing(connectionToken);
      }
      catch (Exception e)
      {
         log.warn("Error closing connection: ", e);
      }
   }
}
