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
import javax.jms.Topic;
import javax.jms.Queue;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public class CreateDestMsg extends BaseMsg
{
   private String name;
   private Destination dest;

   public CreateDestMsg(boolean isQueue)
   {
      this(null, isQueue);
   }
   public CreateDestMsg(String name, boolean isQueue)
   {
      super(isQueue ? MsgTypes.m_createQueue : MsgTypes.m_createTopic);
      this.name = name;
   }

   public String getName()
   {
      return name;
   }
   public Queue getQueue()
   {
      return (Queue) dest;
   }
   public Topic getTopic()
   {
      return (Topic) dest;
   }
   public void setDest(Destination dest)
   {
      this.dest = dest;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeObject(name);
      int hasDest = dest != null ? 1 : 0;
      out.writeByte(hasDest);
      if (hasDest == 1)
         out.writeObject(dest);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      name = (String) in.readObject();
      int hasDest = in.readByte();
      if (hasDest == 1)
         dest = (Destination) in.readObject();
   }
}
