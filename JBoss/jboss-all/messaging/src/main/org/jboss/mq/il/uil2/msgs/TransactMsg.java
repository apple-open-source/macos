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
import org.jboss.mq.TransactionRequest;
import org.jboss.mq.Subscription;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class TransactMsg extends BaseMsg
{
   private TransactionRequest request;

   public TransactMsg()
   {
      this(new TransactionRequest());
   }
   public TransactMsg(TransactionRequest request)
   {
      super(MsgTypes.m_transact);
      this.request = request;
   }

   public TransactionRequest getRequest()
   {
      return request;
   }

   public void trimReply()
   {
      request = null;
   }

   public void write(ObjectOutputStream out) throws IOException
   {
      super.write(out);
      int hasRequest = request != null ? 1 : 0;
      out.writeByte(hasRequest);
      if (hasRequest == 1)
         request.writeExternal(out);
   }
   public void read(ObjectInputStream in) throws IOException, ClassNotFoundException
   {
      super.read(in);
      int hasRequest = in.readByte();
      if (hasRequest == 1)
         request.readExternal(in);
   }
}
