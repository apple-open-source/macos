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
import org.jboss.mq.Subscription;
import org.jboss.mq.ConnectionToken;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class SubscribeMsg extends BaseMsg
{
   private Subscription sub;

   public SubscribeMsg()
   {
      this(null);
   }
   public SubscribeMsg(Subscription sub)
   {
      super(MsgTypes.m_subscribe);
      this.sub = sub;
   }

   public Subscription getSubscription()
   {
      return sub;
   }

   public void trimReply()
   {
      sub = null;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      int hasSub = sub != null ? 1 : 0;
      out.writeByte(hasSub);
      if (hasSub == 1)
         out.writeObject(sub);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      int hasSub = in.readByte();
      if (hasSub == 1)
         sub = (Subscription) in.readObject();
   }
}
