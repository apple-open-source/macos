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
import org.jboss.mq.ConnectionToken;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class ConnectionTokenMsg extends BaseMsg
{
   ConnectionToken token;

   public ConnectionTokenMsg()
   {
      this(null);
   }
   public ConnectionTokenMsg(ConnectionToken token)
   {
      super(MsgTypes.m_setSpyDistributedConnection);
      this.token = token;
   }

   public ConnectionToken getToken()
   {
      return token;
   }

   public void trimReply()
   {
      token = null;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      int hasToken = token != null ? 1 : 0;
      out.writeByte(hasToken);
      if (hasToken == 1)
         out.writeObject(token);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      int hasToken = in.readByte();
      if (hasToken == 1)
         token = (ConnectionToken) in.readObject();
   }
}
