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
import javax.jms.Topic;
import javax.jms.Queue;
import org.jboss.mq.SpyDestination;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class DeleteTemporaryDestMsg extends BaseMsg
{
   private SpyDestination dest;

   public DeleteTemporaryDestMsg()
   {
      this(null);
   }
   public DeleteTemporaryDestMsg(SpyDestination dest)
   {
      super(MsgTypes.m_deleteTemporaryDestination);
      this.dest = dest;
   }

   public SpyDestination getDest()
   {
      return dest;
   }
   public Queue getQueue()
   {
      // Block until the reply is received
      return (Queue) dest;
   }
   public Topic getTopic()
   {
      // Block until the reply is received
      return (Topic) dest;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeObject(dest);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      dest = (SpyDestination) in.readObject();
   }
}
