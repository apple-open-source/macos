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
import org.jboss.mq.DurableSubscriptionID;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class CheckUserMsg extends BaseMsg
{
   private String username;
   private String password;
   private String id;

   public CheckUserMsg(boolean authenticate)
   {
      this(null, null, authenticate);
   }
   public CheckUserMsg(String username, String password, boolean authenticate)
   {
      super(authenticate ? MsgTypes.m_authenticate : MsgTypes.m_checkUser);
      this.username = username;
      this.password = password;
   }

   public String getID()
   {
      return id;
   }
   public void setID(String id)
   {
      this.id = id;
   }
   public String getPassword()
   {
      return password;
   }
   public String getUsername()
   {
      return username;
   }
   public void clearPassword()
   {
      password = null;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      out.writeObject(username);
      out.writeObject(password);
      out.writeObject(id);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      username = (String) in.readObject();
      password = (String) in.readObject();
      id = (String) in.readObject();
   }
}
