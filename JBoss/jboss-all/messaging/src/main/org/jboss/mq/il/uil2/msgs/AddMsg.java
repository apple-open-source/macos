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

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class AddMsg extends BaseMsg
{
   private SpyMessage msg;

   public AddMsg()
   {
      this(null);
   }
   public AddMsg(SpyMessage msg)
   {
      super(MsgTypes.m_addMessage);
      this.msg = msg;
   }

   public SpyMessage getMsg()
   {
      return msg;
   }

   public void trimReply()
   {
      msg = null;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      int hasMessage = msg != null ? 1 : 0;
      out.writeByte(hasMessage);
      if (hasMessage == 1)
         SpyMessage.writeMessage(msg, out);
   }

   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      int hasMessage = in.readByte();
      if (hasMessage == 1)
         msg = SpyMessage.readMessage(in);
   }

}
