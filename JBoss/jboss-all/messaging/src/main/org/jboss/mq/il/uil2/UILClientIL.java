/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2;

import java.io.Serializable;

import org.jboss.logging.Logger;
import org.jboss.mq.ReceiveRequest;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.uil2.msgs.MsgTypes;
import org.jboss.mq.il.uil2.msgs.DeleteTemporaryDestMsg;
import org.jboss.mq.il.uil2.msgs.PingMsg;
import org.jboss.mq.il.uil2.msgs.ReceiveRequestMsg;

/** UILClient is the server side interface for callbacks into the client. It
 * is created on the client and sent to the server via the ConnectionToken.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class UILClientIL
   implements ClientIL, MsgTypes, Serializable
{
   static Logger log = Logger.getLogger(UILClientIL.class);

   private transient SocketManager socketMgr;

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   public void close()
          throws Exception
   {
   }

   /**
    * #Description of the Method
    *
    * @param dest           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void deleteTemporaryDestination(SpyDestination dest)
          throws Exception
   {
      DeleteTemporaryDestMsg msg = new DeleteTemporaryDestMsg(dest);
      socketMgr.sendReply(msg);
   }

   /**
    * #Description of the Method
    *
    * @param serverTime     Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void pong(long serverTime)
          throws Exception
   {
      PingMsg msg = new PingMsg(serverTime, false);
      msg.getMsgID();      
      socketMgr.sendReply(msg);
   }

   /**
    * #Description of the Method
    *
    * @param messages       Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void receive(ReceiveRequest messages[])
          throws Exception
   {
      ReceiveRequestMsg msg = new ReceiveRequestMsg(messages);
      socketMgr.sendMessage(msg);
   }

   protected void setSocketMgr(SocketManager socketMgr)
   {
      this.socketMgr = socketMgr;
   }
}
