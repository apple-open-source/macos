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
import org.jboss.mq.SpyDestination;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class EnableConnectionMsg extends BaseMsg
{
   private boolean enabled;

   public EnableConnectionMsg()
   {
      this(false);
   }
   public EnableConnectionMsg(boolean enabled)
   {
      super(MsgTypes.m_setEnabled);
      this.enabled = enabled;
   }

   public boolean isEnabled()
   {
      return enabled;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeBoolean(enabled);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      enabled = in.readBoolean();
   }
}
