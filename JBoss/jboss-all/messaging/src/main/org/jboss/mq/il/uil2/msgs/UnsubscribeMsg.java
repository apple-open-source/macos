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

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class UnsubscribeMsg extends BaseMsg
{
   private int subscriptionID;

   public UnsubscribeMsg()
   {
      this(0);
   }
   public UnsubscribeMsg(int subscriptionID)
   {
      super(MsgTypes.m_unsubscribe);
      this.subscriptionID = subscriptionID;
   }

   public int getSubscriptionID()
   {
      return subscriptionID;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeInt(subscriptionID);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      subscriptionID = in.readInt();
   }
}
