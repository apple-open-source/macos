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
import javax.jms.Destination;
import org.jboss.mq.SpyMessage;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class BrowseMsg extends BaseMsg
{
   private Destination dest;
   private String selector;
   private SpyMessage[] msgs;

   public BrowseMsg()
   {
      this(null, null);
   }
   public BrowseMsg(Destination dest, String selector)
   {
      super(MsgTypes.m_browse);
      this.dest = dest;
      this.selector = selector;
   }

   public Destination getDest()
   {
      return dest;
   }
   public String getSelector()
   {
      return selector;
   }
   public void setMessages(SpyMessage[] msgs)
   {
      this.msgs = msgs;
   }
   public SpyMessage[] getMessages()
   {
      return msgs;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeObject(dest);
      out.writeObject(selector);
      int hasMessages = msgs != null ? 1 : 0;
      out.writeByte(hasMessages);
      if (hasMessages == 1)
         out.writeObject(msgs);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      dest = (Destination) in.readObject();
      selector = (String) in.readObject();
      int hasMessages = in.readByte();
      if (hasMessages == 1)
         msgs = (SpyMessage[]) in.readObject();
   }
}
