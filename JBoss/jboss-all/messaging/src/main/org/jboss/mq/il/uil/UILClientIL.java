/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;

import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.rmi.RemoteException;

import javax.jms.Destination;
import javax.jms.JMSException;

import org.jboss.logging.Logger;
import org.jboss.mq.Connection;
import org.jboss.mq.ReceiveRequest;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.uil.multiplexor.SocketMultiplexor;

/**
 * The RMI implementation of the ConnectionReceiver object
 *
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @version   $Revision: 1.4.4.2 $
 * @created   August 16, 2001
 */
public class UILClientIL implements ClientIL, java.io.Serializable
{
   static Logger log = Logger.getLogger(UILClientIL.class);
   final static int m_close = 2;
   final static int m_deleteTemporaryDestination = 1;
   final static int m_receive = 3;
   final static int m_pong = 4;
   transient SocketMultiplexor mSocket;
   private transient ObjectInputStream in;
   private transient ObjectOutputStream out;

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   public void close()
          throws Exception
   {
   }

   /**
    * #Description of the Method
    *
    * @param dest           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public synchronized void deleteTemporaryDestination(SpyDestination dest)
          throws Exception
   {
      checkSocket();
      out.writeByte(m_deleteTemporaryDestination);
      out.writeObject(dest);
      waitAnswer();
   }

   /**
    * #Description of the Method
    *
    * @param serverTime     Description of Parameter
    * @exception Exception  Description of Exception
    */
   public synchronized void pong(long serverTime)
          throws Exception
   {
      checkSocket();
      out.writeByte(m_pong);
      out.writeLong(serverTime);
      out.flush();
      /* This does not call waitAnswer to block on a reply since this
      holds the UILClientIL mutex and can result in a deadlock
      */
   }

   /**
    * #Description of the Method
    *
    * @param messages       Description of Parameter
    * @exception Exception  Description of Exception
    */
   public synchronized void receive(ReceiveRequest messages[])
          throws Exception
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("Checking socket");
      checkSocket();
      if( trace )
         log.trace("Writing request");
      out.writeByte(m_receive);
      out.writeInt(messages.length);
      for (int i = 0; i < messages.length; ++i)
      {
         messages[i].writeExternal(out);
      }
      if( trace )
         log.trace("Waiting for awnser");
      waitAnswer();
      if( trace )
         log.trace("Done");
   }

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   protected void checkSocket()
          throws Exception
   {
      if (out == null)
      {
         createConnection();
      }
   }

   /**
    * #Description of the Method
    *
    * @exception RemoteException  Description of Exception
    */
   protected void createConnection()
          throws RemoteException
   {
      try
      {
         in = new ObjectInputStream(new BufferedInputStream(mSocket.getInputStream(2)));
         out = new ObjectOutputStream(new BufferedOutputStream(mSocket.getOutputStream(2)));
         out.flush();
      }
      catch (Exception e)
      {
         log.debug("Cannot connect to the ConnectionReceiver/Server", e);
         throw new RemoteException("Cannot connect to the ConnectionReceiver/Server");
      }
   }

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   protected void waitAnswer()
          throws Exception
   {
      Exception throwException = null;
      try
      {
         out.reset();
         out.flush();
         int val = in.readByte();
         switch (val)
         {
            case 1:
               Exception e = (Exception)in.readObject();
               throwException = new RemoteException("", e);
               break;
         }
      }
      catch (IOException e)
      {
         throw new RemoteException("Cannot contact the remote object", e);
      }

      if (throwException != null)
      {
         throw throwException;
      }
   }
}
