/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2;

import java.lang.reflect.UndeclaredThrowableException;
import java.util.Properties;

import org.jboss.logging.Logger;
import org.jboss.mq.Connection;
import org.jboss.mq.ReceiveRequest;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientILService;
import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.uil2.msgs.MsgTypes;
import org.jboss.mq.il.uil2.msgs.BaseMsg;
import org.jboss.mq.il.uil2.msgs.ReceiveRequestMsg;
import org.jboss.mq.il.uil2.msgs.DeleteTemporaryDestMsg;
import org.jboss.mq.il.uil2.msgs.PingMsg;

/** The UILClientILService runs on the client side of a JMS server connection
 * and acts as a factory for the UILClientIL passed to the server. It also
 * handles the callbacks from the client side SocketManager.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public class UILClientILService
      implements ClientILService, MsgTypes, SocketManagerHandler
{
   static Logger log = Logger.getLogger(UILClientILService.class);

   // Attributes ----------------------------------------------------

   //the client IL
   private UILClientIL clientIL;
   //The thread that is doing the Socket reading work
   private SocketManager socketMgr;
   //A link on my connection
   private Connection connection;

   /**
    * getClientIL method comment.
    *
    * @return  The ClientIL value
    * @exception Exception  Description of Exception
    */
   public ClientIL getClientIL()
         throws Exception
   {
      return clientIL;
   }

   /**
    * init method comment.
    *
    * @param connection               Description of Parameter
    * @param props                    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void init(Connection connection, Properties props)
         throws Exception
   {
      this.connection = connection;
      clientIL = new UILClientIL();
      UILServerIL serverIL = (UILServerIL) connection.getServerIL();
      socketMgr = serverIL.getSocketMgr();
      String t = props.getProperty(UILServerILFactory.UIL_BUFFERSIZE_KEY);
      if (t != null)
         socketMgr.setBufferSize(Integer.parseInt(t));
      t = props.getProperty(UILServerILFactory.UIL_CHUNKSIZE_KEY);
      if (t != null)
         socketMgr.setChunkSize(Integer.parseInt(t));
      socketMgr.setHandler(this);
   }

   /** Callback from the SocketManager
    */
   public void handleMsg(BaseMsg msg)
      throws Exception
   {
      boolean trace = log.isTraceEnabled();
      int msgType = msg.getMsgType();
      if (trace)
         log.trace("Begin handleMsg, msgType: " + msgType);
      switch( msgType )
      {
         case m_receiveRequest:
            ReceiveRequestMsg rmsg = (ReceiveRequestMsg) msg;
            ReceiveRequest[] messages = rmsg.getMessages();
            connection.asynchDeliver(messages);
            socketMgr.sendReply(msg);
            break;
         case m_deleteTemporaryDestination:
            DeleteTemporaryDestMsg dmsg = (DeleteTemporaryDestMsg) msg;
            SpyDestination dest = dmsg.getDest();
            connection.asynchDeleteTemporaryDestination(dest);
            socketMgr.sendReply(msg);
            break;
         case m_close:
            connection.asynchClose();
            socketMgr.sendReply(msg);
            break;
         case m_pong:
            PingMsg pmsg = (PingMsg) msg;
            long time = pmsg.getTime();
            connection.asynchPong(time);
            break;
         default:
            connection.asynchFailure("UILClientILService received bad msg: "+msg, null);
      }
      if (trace)
         log.trace("End handleMsg");
   }

   /**
    *
    * @exception Exception  Description of Exception
    */
   public void start() throws Exception
   {
      log.debug("Starting");
   }

   /**
    * @exception Exception  Description of Exception
    */
   public void stop() throws Exception
   {
      log.debug("Stopping");
      socketMgr.stop();
   }

   public void onStreamNotification(Object stream, int size)
   {
      connection.asynchPong(System.currentTimeMillis());
   }

   public void asynchFailure(String error, Throwable e)
   {
      if (e instanceof Exception)
         connection.asynchFailure(error, (Exception) e);
      else
         connection.asynchFailure(error, new UndeclaredThrowableException(e));
   }

   public void close()
   {
   }
}
