/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2.msgs;

import java.io.ObjectOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.lang.reflect.UndeclaredThrowableException;

import org.jboss.mq.il.uil2.SocketManager.ReadTask;

/** The base msg class for all msgs used by the UIL2 invoker. Msgs consist
 * of a msg type, id and exception and can operate as two way items that
 * are sent with the request content and received with the reply content for
 * the request. Such round-trip behavior is based on matching the request
 * msgID with the reply msgID. The msgID parameter is segmented into value
 * 1 to 2147483647 for client originated msgs and -1 to -2147483647 for server
 * originated msgs.
 *
 * <p>The message is a Runnable to avoid constructing a Runnable object
 * when asynchronously handling the message from the ReadTask.
 *
 * @author Scott.Stark@jboss.org
 * @author Adrian.Brock@HappeningTimes.com
 * @version $Revision: 1.1.4.3 $
 */
public class BaseMsg
   implements Runnable
{
   /** A flag indicating if the msgIDs are used by the JMS server */
   private static boolean useJMSServerMsgIDs = false;
   /** The next base msgID */
   private static int nextMsgID = 0;
   /** The lock for the next message id */
   private static Object nextMsgIDLock = new Object();
   /** 2^31+1 */
   private static final int SERVER_MSG_ID_MASK = 0x80000000;

   /** The handler of this message */
   private ReadTask handler;

   /** The MsgTypes constant representing the type of the msg */
   public int msgType;
   /** A msg id used to associated a reply with its request */
   public int msgID;
   /** Any error thrown by the remote side */
   public Exception error;

   public BaseMsg(int msgType)
   {
      this(msgType, 0);
   }
   public BaseMsg(int msgType, int msgID)
   {
      this.msgType = msgType;
      this.msgID = msgID;
   }

   /** Set the msgID parameter range. If false, the msgID is segmented into the
    * range 1 to 2147483647 and if true, the rangs is -1 to -2147483647. The
    * JMS server sets this to true and clients default to false.
    * @param flag
    */
   public static void setUseJMSServerMsgIDs(boolean flag)
   {
      useJMSServerMsgIDs = flag;
   }

   /** Create a BaseMsg subclass based on the msgType.
    *
    * @param msgType A MsgTypes.m_xxx constant
    * @return the derived BaseMsg
    * @throws IllegalArgumentException thrown for a msgType that does not
    * match any MsgTypes.m_xxx constant
    */
   public static BaseMsg createMsg(int msgType) throws IllegalArgumentException
   {
      BaseMsg msg = null;
      switch( msgType )
      {
         case MsgTypes.m_acknowledge:
            msg = new AcknowledgementRequestMsg();
            break;
         case MsgTypes.m_addMessage:
            msg = new AddMsg();
            break;
         case MsgTypes.m_browse:
            msg = new BrowseMsg();
            break;
         case MsgTypes.m_checkID:
            msg = new CheckIDMsg();
            break;
         case MsgTypes.m_connectionClosing:
            msg = new CloseMsg();
            break;
         case MsgTypes.m_createQueue:
            msg = new CreateDestMsg(true);
            break;
         case MsgTypes.m_createTopic:
            msg = new CreateDestMsg(false);
            break;
         case MsgTypes.m_deleteTemporaryDestination:
            msg = new DeleteTemporaryDestMsg();
            break;
         case MsgTypes.m_getID:
            msg = new GetIDMsg();
            break;
         case MsgTypes.m_getTemporaryQueue:
            msg = new TemporaryDestMsg(true);
            break;
         case MsgTypes.m_getTemporaryTopic:
            msg = new TemporaryDestMsg(false);
            break;
         case MsgTypes.m_receive:
            msg = new ReceiveMsg();
            break;
         case MsgTypes.m_setEnabled:
            msg = new EnableConnectionMsg();
            break;
         case MsgTypes.m_setSpyDistributedConnection:
            msg = new ConnectionTokenMsg();
            break;
         case MsgTypes.m_subscribe:
            msg = new SubscribeMsg();
            break;
         case MsgTypes.m_transact:
            msg = new TransactMsg();
            break;
         case MsgTypes.m_unsubscribe:
            msg = new UnsubscribeMsg();
            break;
         case MsgTypes.m_destroySubscription:
            msg = new DeleteSubscriptionMsg();
            break;
         case MsgTypes.m_checkUser:
            msg = new CheckUserMsg(false);
            break;
         case MsgTypes.m_ping:
            msg = new PingMsg(true);
            break;
         case MsgTypes.m_authenticate:
            msg = new CheckUserMsg(true);
            break;
         case MsgTypes.m_close:
            // This is never sent
            break;
         case MsgTypes.m_pong:
            msg = new PingMsg(false);
            break;
         case MsgTypes.m_receiveRequest:
            msg = new ReceiveRequestMsg();
            break;
         default:
            throw new IllegalArgumentException("Invalid msgType: "+msgType);
      }
      return msg;
   }

   /** Translate a msgType into its string menmonic.
    * @param msgType A MsgTypes.m_xxx constant
    * @return the string form of the MsgTypes.m_xxx constant
    */
   public static String toString(int msgType)
   {
      String msgTypeString = null;
      switch (msgType)
      {
         case MsgTypes.m_acknowledge:
            msgTypeString = "m_acknowledge";
            break;
         case MsgTypes.m_addMessage:
            msgTypeString = "m_addMessage";
            break;
         case MsgTypes.m_browse:
            msgTypeString = "m_browse";
            break;
         case MsgTypes.m_checkID:
            msgTypeString = "m_checkID";
            break;
         case MsgTypes.m_connectionClosing:
            msgTypeString = "m_connectionClosing";
            break;
         case MsgTypes.m_createQueue:
            msgTypeString = "m_createQueue";
            break;
         case MsgTypes.m_createTopic:
            msgTypeString = "m_createTopic";
            break;
         case MsgTypes.m_deleteTemporaryDestination:
            msgTypeString = "m_deleteTemporaryDestination";
            break;
         case MsgTypes.m_getID:
            msgTypeString = "m_getID";
            break;
         case MsgTypes.m_getTemporaryQueue:
            msgTypeString = "m_getTemporaryQueue";
            break;
         case MsgTypes.m_getTemporaryTopic:
            msgTypeString = "m_getTemporaryTopic";
            break;
         case MsgTypes.m_receive:
            msgTypeString = "m_receive";
            break;
         case MsgTypes.m_setEnabled:
            msgTypeString = "m_setEnabled";
            break;
         case MsgTypes.m_setSpyDistributedConnection:
            msgTypeString = "m_setSpyDistributedConnection";
            break;
         case MsgTypes.m_subscribe:
            msgTypeString = "m_subscribe";
            break;
         case MsgTypes.m_transact:
            msgTypeString = "m_transact";
            break;
         case MsgTypes.m_unsubscribe:
            msgTypeString = "m_unsubscribe";
            break;
         case MsgTypes.m_destroySubscription:
            msgTypeString = "m_destroySubscription";
            break;
         case MsgTypes.m_checkUser:
            msgTypeString = "m_checkUser";
            break;
         case MsgTypes.m_ping:
            msgTypeString = "m_ping";
            break;
         case MsgTypes.m_authenticate:
            msgTypeString = "m_authenticate";
            break;
         case MsgTypes.m_close:
            msgTypeString = "m_close";
            break;
         case MsgTypes.m_pong:
            msgTypeString = "m_pong";
            break;
         case MsgTypes.m_receiveRequest:
            msgTypeString = "m_receiveRequest";
            break;
         default:
            msgTypeString = "unknown message type " + msgType;
      }
      return msgTypeString;
   }

   public int getMsgType()
   {
      return msgType;
   }

   /** Access the msgID, initializing it if it has not been set yet. This
    * is used by the SocketManager.internalSendMessage to setup the unique
    * msgID for a request msg.
    *
    * @return the msgID value
    */
   public synchronized int getMsgID()
   {
      if( msgID == 0 )
      {
         synchronized (nextMsgIDLock)
         {
            msgID = ++ nextMsgID;
         }
         if( useJMSServerMsgIDs )
            msgID += SERVER_MSG_ID_MASK;
         else if( msgID >= SERVER_MSG_ID_MASK )
            msgID = msgID % SERVER_MSG_ID_MASK;
      }
      return msgID;
   }
   /** Set the msgID. This is used by the SocketManager read task to populate
    * a msg with its request ID.
    * @param msgID the msgID read off the socket
    */
   public void setMsgID(int msgID)
   {
      this.msgID = msgID;
   }

   /** Access any exception associated with the msg
    * @return
    */
   public Exception getError()
   {
      return error;
   }
   /** Set an exception that should be used as the msg return value.
    *
    * @param e
    */
   public void setError(Throwable e)
   {
      if( e instanceof Exception )
         error = (Exception) e;
      else
         error = new UndeclaredThrowableException(e);
   }

   /** Equality is based on BaseMsg.msgID
    * @param o a BaseMsg
    * @return true if o.msgID == msgID
    */
   public boolean equals(Object o)
   {
      BaseMsg msg = (BaseMsg) o;
      return msg.msgID == msgID;
   }

   /** Hash code is simply the msgID
    * @return
    */
   public int hashCode()
   {
      return msgID;
   }

   public String toString()
   {
      StringBuffer tmp = new StringBuffer(this.getClass().getName());
      tmp.append(System.identityHashCode(this));
      tmp.append("[msgType: ");
      tmp.append(toString(msgType));
      tmp.append(", msgID: ");
      tmp.append(msgID);
      tmp.append(", error: ");
      tmp.append(error);
      tmp.append("]");
      return tmp.toString();
   }

   /** Trim the message when replying
    */
   public void trimReply()
   {
   }

   /** Write the msgType, msgID, hasError flag and optionally the error
    * @param out
    * @throws IOException
    */
   public void write(ObjectOutputStream out) throws IOException
   {
      out.writeByte(msgType);
      out.writeInt(msgID);
      int hasError = error != null ? 1 : 0;
      out.writeByte(hasError);
      if( hasError == 1 )
         out.writeObject(error);
   }
   /** Read the hasError flag and optionally the error. This method is not
    * a complete analog of write because the SocketManager read task reads
    * the msgType and msgID off of the socket.
    *
    * @param in
    * @throws IOException
    * @throws ClassNotFoundException
    */
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      int hasError = in.readByte();
      if( hasError == 1 )
         error = (Exception) in.readObject();
   }

   public void setHandler(ReadTask handler)
   {
      this.handler = handler;
   }

   public void run()
   {
      handler.handleMsg(this);
      handler = null;
   }
}
