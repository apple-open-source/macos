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
import javax.jms.TemporaryQueue;
import javax.jms.Destination;
import javax.jms.TemporaryTopic;
import org.jboss.mq.ConnectionToken;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class TemporaryDestMsg extends BaseMsg
{
   private Destination dest;

   public TemporaryDestMsg(boolean isQueue)
   {
      super(isQueue ? MsgTypes.m_getTemporaryQueue : MsgTypes.m_getTemporaryTopic);
   }

   public TemporaryQueue getQueue()
   {
      return (TemporaryQueue) dest;
   }
   public TemporaryTopic getTopic()
   {
      return (TemporaryTopic) dest;
   }
   public void setDest(Destination dest)
   {
      this.dest = dest;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeObject(dest);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      dest = (Destination) in.readObject();
   }
}
