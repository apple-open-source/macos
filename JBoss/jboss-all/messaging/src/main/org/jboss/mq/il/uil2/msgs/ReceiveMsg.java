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
import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyDestination;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class ReceiveMsg extends BaseMsg
{
   private int subscriberID;
   private long wait;
   private SpyMessage msg;

   public ReceiveMsg()
   {
      this(0, 0);
   }
   public ReceiveMsg(int subscriberID, long wait)
   {
      super(MsgTypes.m_receive);
      this.subscriberID = subscriberID;
      this.wait = wait;
   }

   public int getSubscriberID()
   {
      return subscriberID;
   }
   public long getWait()
   {
      return wait;
   }
   public SpyMessage getMessage()
   {
      // Block until the reply is received
      return msg;
   }
   public void setMessage(SpyMessage msg)
   {
      this.msg = msg;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeInt(subscriberID);
      out.writeLong(wait);
      out.writeByte(msg != null ? 1 : 0);
      if( msg != null )
         SpyMessage.writeMessage(msg, out);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      subscriberID = in.readInt();
      wait = in.readLong();
      int hasMsg = in.readByte();
      if( hasMsg == 1 )
         msg = SpyMessage.readMessage(in);
   }

}
