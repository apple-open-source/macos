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
import org.jboss.mq.DurableSubscriptionID;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class DeleteSubscriptionMsg extends BaseMsg
{
   private DurableSubscriptionID id;

   public DeleteSubscriptionMsg()
   {
      this(null);
   }
   public DeleteSubscriptionMsg(DurableSubscriptionID id)
   {
      super(MsgTypes.m_destroySubscription);
      this.id = id;
   }

   public DurableSubscriptionID getSubscriptionID()
   {
      return id;
   }

   public void trimReply()
   {
      id = null;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      int hasId = id != null ? 1 : 0;
      out.writeByte(hasId);
      if (hasId == 1)
         out.writeObject(id);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      int hasId = in.readByte();
      if (hasId == 1)
         id = (DurableSubscriptionID) in.readObject();
   }
}
