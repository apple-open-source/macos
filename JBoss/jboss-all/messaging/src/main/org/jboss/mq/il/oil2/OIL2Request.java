/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil2;

import java.io.Externalizable;
import java.io.IOException;

import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.ReceiveRequest;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.TransactionRequest;

/**
 * 
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision: 1.2 $
 */
public class OIL2Request implements Externalizable
{
   private static int lastRequestId = 0;
   private static Object lastRequestIdLock = new Object();
   public Integer requestId;
   public byte operation;
   public Object arguments[];
   
   
   public OIL2Request() {}
   public OIL2Request(byte operation, Object[] arguments)
   {
      synchronized (lastRequestIdLock)
      {
         this.requestId = new Integer(lastRequestId++);
      }
      this.operation = operation;
      this.arguments = arguments;
   }

   public void writeExternal(java.io.ObjectOutput out) throws java.io.IOException
   {
      out.writeByte(operation);
      out.writeInt(requestId.intValue());
      switch (operation)
      {
         
         //////////////////////////////////////////////////////////////////
         // These are the requests that the Server makes to the client
         //////////////////////////////////////////////////////////////////
         case OIL2Constants.CLIENT_RECEIVE :
            ReceiveRequest[] messages = (ReceiveRequest[]) arguments[0];
            out.writeInt(messages.length);
            for (int i = 0; i < messages.length; ++i)
               messages[i].writeExternal(out);
            break;

         case OIL2Constants.CLIENT_DELETE_TEMPORARY_DESTINATION :
            out.writeObject(arguments[0]);
            break;

         case OIL2Constants.CLIENT_CLOSE :
            break;

         case OIL2Constants.CLIENT_PONG :
            out.writeLong(((Long) arguments[0]).longValue());
            break;


         //////////////////////////////////////////////////////////////////
         // These are the requests that the client makes to the server
         //////////////////////////////////////////////////////////////////
         case OIL2Constants.SERVER_SET_SPY_DISTRIBUTED_CONNECTION :
            out.writeObject(arguments[0]);
            break;

         case OIL2Constants.SERVER_ACKNOWLEDGE :
            ((AcknowledgementRequest)arguments[0]).writeExternal(out);
            break;

         case OIL2Constants.SERVER_ADD_MESSAGE :
            SpyMessage.writeMessage(((SpyMessage)arguments[0]), out);
            break;

         case OIL2Constants.SERVER_BROWSE :
            out.writeObject(arguments[0]);
            writeString(out,(String)arguments[1]);
            break;

         case OIL2Constants.SERVER_CHECK_ID :
            writeString(out,(String)arguments[0]);
            break;

         case OIL2Constants.SERVER_CONNECTION_CLOSING :
            arguments = null;
            break;

         case OIL2Constants.SERVER_CREATE_QUEUE :
            writeString(out,(String)arguments[0]);
            break;

         case OIL2Constants.SERVER_CREATE_TOPIC :
            writeString(out,(String)arguments[0]);
            break;

         case OIL2Constants.SERVER_GET_ID :
            break;

         case OIL2Constants.SERVER_GET_TEMPORARY_QUEUE :
            break;

         case OIL2Constants.SERVER_GET_TEMPORARY_TOPIC :
            break;
            
         case OIL2Constants.SERVER_DELETE_TEMPORARY_DESTINATION :
            out.writeObject(arguments[0]);
            break;

         case OIL2Constants.SERVER_RECEIVE :
            out.writeInt(((Integer)arguments[0]).intValue());
            out.writeLong(((Long)arguments[1]).longValue());
            break;

         case OIL2Constants.SERVER_SET_ENABLED :
            out.writeBoolean(((Boolean)arguments[0]).booleanValue());
            break;

         case OIL2Constants.SERVER_SUBSCRIBE :
            out.writeObject(arguments[0]);
            break;

         case OIL2Constants.SERVER_TRANSACT :
            ((TransactionRequest)arguments[0]).writeExternal(out);
            break;

         case OIL2Constants.SERVER_UNSUBSCRIBE :
            out.writeInt(((Integer)arguments[0]).intValue());
            break;

         case OIL2Constants.SERVER_DESTROY_SUBSCRIPTION :
            out.writeObject(arguments[0]);
            break;

         case OIL2Constants.SERVER_CHECK_USER :
            writeString(out,(String)arguments[0]);
            writeString(out,(String)arguments[1]);
            break;

         case OIL2Constants.SERVER_PING :
            out.writeLong(((Long)arguments[0]).longValue());
            break;

         case OIL2Constants.SERVER_AUTHENTICATE :
            writeString(out,(String)arguments[0]);
            writeString(out,(String)arguments[1]);
            break;

         default :
            throw new IOException("Protocol Error: Bad operation code.");
      }
   }

   public void readExternal(java.io.ObjectInput in) throws java.io.IOException, ClassNotFoundException
   {
      operation = in.readByte();
      requestId = new Integer(in.readInt());
      switch (operation)
      {
         //////////////////////////////////////////////////////////////////
         // These are the requests that the Server makes to the client
         //////////////////////////////////////////////////////////////////
         case OIL2Constants.CLIENT_RECEIVE :
            int numReceives = in.readInt();
            org.jboss.mq.ReceiveRequest[] messages = new org.jboss.mq.ReceiveRequest[numReceives];
            for (int i = 0; i < numReceives; ++i)
            {
               messages[i] = new ReceiveRequest();
               messages[i].readExternal(in);
            }
            arguments = new Object[] { messages };
            break;

         case OIL2Constants.CLIENT_DELETE_TEMPORARY_DESTINATION :
            arguments = new Object[] { in.readObject()};
            break;

         case OIL2Constants.CLIENT_CLOSE :
            arguments = null;
            break;

         case OIL2Constants.CLIENT_PONG :
            arguments = new Object[] { new Long(in.readLong())};
            break;
            
         //////////////////////////////////////////////////////////////////
         // These are the requests that the client makes to the server
         //////////////////////////////////////////////////////////////////
         case OIL2Constants.SERVER_SET_SPY_DISTRIBUTED_CONNECTION :
            // assert connectionToken == null
            arguments = new Object[] {in.readObject()};
            break;

         case OIL2Constants.SERVER_ACKNOWLEDGE :
            AcknowledgementRequest ack = new AcknowledgementRequest();
            ack.readExternal(in);
            arguments = new Object[] {ack};
            break;

         case OIL2Constants.SERVER_ADD_MESSAGE :
            arguments = new Object[] { SpyMessage.readMessage(in) };
            break;

         case OIL2Constants.SERVER_BROWSE :
            arguments = new Object[] {in.readObject(), readString(in)};
            break;

         case OIL2Constants.SERVER_CHECK_ID :
            arguments = new Object[] { readString(in) };
            break;

         case OIL2Constants.SERVER_CONNECTION_CLOSING :
            arguments = null;
            break;

         case OIL2Constants.SERVER_CREATE_QUEUE :
            arguments = new Object[] { readString(in) };
            break;

         case OIL2Constants.SERVER_CREATE_TOPIC :
            arguments = new Object[] { readString(in) };
            break;

         case OIL2Constants.SERVER_GET_ID :
            arguments = null;
            break;

         case OIL2Constants.SERVER_GET_TEMPORARY_QUEUE :
            arguments = null;
            break;

         case OIL2Constants.SERVER_GET_TEMPORARY_TOPIC :
            arguments = null;
            break;
            
         case OIL2Constants.SERVER_DELETE_TEMPORARY_DESTINATION :
            arguments = new Object[] { in.readObject()};
            break;

         case OIL2Constants.SERVER_RECEIVE :
            arguments = new Object[] { new Integer(in.readInt()), new Long(in.readLong())};
            break;

         case OIL2Constants.SERVER_SET_ENABLED :
            arguments = new Object[] { new Boolean(in.readBoolean()) };
            break;

         case OIL2Constants.SERVER_SUBSCRIBE :
            arguments = new Object[] {in.readObject()};
            break;

         case OIL2Constants.SERVER_TRANSACT :
            TransactionRequest trans = new TransactionRequest();
            trans.readExternal(in);
            arguments = new Object[] {trans};
            break;

         case OIL2Constants.SERVER_UNSUBSCRIBE :
            arguments = new Object[] {new Integer(in.readInt())};
            break;

         case OIL2Constants.SERVER_DESTROY_SUBSCRIPTION :
            arguments = new Object[] {in.readObject()};
            break;

         case OIL2Constants.SERVER_CHECK_USER :
            arguments = new Object[] {readString(in), readString(in)};
            break;

         case OIL2Constants.SERVER_PING :
            arguments = new Object[] {new Long(in.readLong())};
            break;

         case OIL2Constants.SERVER_AUTHENTICATE :
            arguments = new Object[] { readString(in), readString(in) };
            break;

         default :
            throw new IOException("Protocol Error: Bad operation code.");
      }
   }
   
   private static void writeString(java.io.ObjectOutput out, String s) throws java.io.IOException {
      if (s == null) {
         out.writeByte(0);
      } else {
         out.writeByte(1);
         out.writeUTF(s);
      }
   }

   private static String readString(java.io.ObjectInput in) throws java.io.IOException {
      byte b = in.readByte();
      if (b == 0)
         return null;
      else
         return in.readUTF();
   }

   public String toString() {
      return "[operation:"+operation+","+"requestId:"+requestId+",arguments:"+arguments+"]";
   }
}
