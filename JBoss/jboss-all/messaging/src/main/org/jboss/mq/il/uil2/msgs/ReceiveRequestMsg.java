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
import org.jboss.mq.ReceiveRequest;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class ReceiveRequestMsg extends BaseMsg
{
   private ReceiveRequest[] messages;

   public ReceiveRequestMsg()
   {
      this(null);
   }
   public ReceiveRequestMsg(ReceiveRequest[] messages)
   {
      super(MsgTypes.m_receiveRequest);
      this.messages = messages;
   }

   public ReceiveRequest[] getMessages()
   {
      return messages;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeInt(messages.length);
      for (int i = 0; i < messages.length; ++i)
      {
         messages[i].writeExternal(out);
      }
   }

   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      int count = in.readInt();
      messages = new ReceiveRequest[count];
      for (int i = 0; i < count; ++i)
      {
         messages[i] = new ReceiveRequest();
         messages[i].readExternal(in);
      }
   }

}
