/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2.msgs;

import java.io.ObjectInputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class PingMsg extends BaseMsg
{
   private long time;

   public PingMsg(boolean isPing)
   {
      this(0, isPing);
   }
   public PingMsg(long time, boolean isPing)
   {
      super(isPing ? MsgTypes.m_ping : MsgTypes.m_pong);
      this.time = time;
   }

   public long getTime()
   {
      return time;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeLong(time);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      time = in.readLong();
   }
}
