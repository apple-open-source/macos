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
import org.jboss.mq.AcknowledgementRequest;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class AcknowledgementRequestMsg extends BaseMsg
{
   private AcknowledgementRequest item;

   public AcknowledgementRequestMsg()
   {
      this(new AcknowledgementRequest());
   }
   public AcknowledgementRequestMsg(AcknowledgementRequest item)
   {
      super(MsgTypes.m_acknowledge);
      this.item = item;
   }

   public AcknowledgementRequest getAck()
   {
      return item;
   }

   public void trimReply()
   {
      item = null;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      int hasItem = item != null ? 1 : 0;
      out.writeByte(hasItem);
      if (hasItem == 1)
         item.writeExternal(out);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      int hasItem = in.readByte();
      if (hasItem == 1)
         item.readExternal(in);
   }
}
