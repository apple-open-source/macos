/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.InetAddress;

import java.net.Socket;
import java.rmi.RemoteException;

import javax.jms.Destination;
import javax.jms.JMSException;

import org.jboss.logging.Logger;
import org.jboss.mq.Connection;
import org.jboss.mq.ReceiveRequest;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientIL;

/**
 * The RMI implementation of the ConnectionReceiver object
 *
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @version   $Revision: 1.9 $
 * @created   August 16, 2001
 */
public final class OILClientIL
   implements ClientIL,
      java.io.Serializable
{
   private final static Logger log = Logger.getLogger(OILClientIL.class);

   private InetAddress addr;
   private int port;
   /**
    * If the TcpNoDelay option should be used on the socket.
    */
   private boolean enableTcpNoDelay=false;
   
   
   private transient ObjectInputStream in;
   private transient ObjectOutputStream out;
   private transient Socket socket;

   OILClientIL(InetAddress addr, int port, boolean enableTcpNoDelay)
   {
      this.addr = addr;
      this.port = port;
      this.enableTcpNoDelay = enableTcpNoDelay;
   }

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   public synchronized void close()
          throws Exception
   {
      if ( log.isTraceEnabled())
         log.trace("Closing OILClientIL");
      checkSocket();
      out.writeByte(OILConstants.CLOSE);
      waitAnswer();
      try
      {
         socket.close();
         in.close();
         out.close();
      }
      catch(Exception e)
      {
         if(log.isDebugEnabled())
            log.debug("Error closing the socket connection", e);
      }
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
      out.writeByte(OILConstants.DELETE_TEMPORARY_DESTINATION);
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
      out.writeByte(OILConstants.PONG);
      out.writeLong(serverTime);
      waitAnswer();
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
      out.writeByte(OILConstants.RECEIVE);
      out.writeInt(messages.length);
      for (int i = 0; i < messages.length; ++i)
      {
         messages[i].writeExternal(out);
      }
      if( trace )
         log.trace("Waiting for anwser");
      waitAnswer();
      if( trace )
         log.trace("Done");
   }

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   private void checkSocket()
          throws RemoteException
   {
      if (socket == null)
      {
         createConnection();
      }
   }

   /**
    * #Description of the Method
    *
    * @exception RemoteException  Description of Exception
    */
   private void createConnection()
          throws RemoteException
   {
      try
      {
	 if (log.isDebugEnabled()) {
	    log.debug("ConnectionReceiverOILClient is connecting to: " + 
		      addr.getHostAddress() + ":" + port);
	 }

         socket = new Socket(addr, port);
         out = new ObjectOutputStream(new BufferedOutputStream(socket.getOutputStream()));
         out.flush();
         in = new ObjectInputStream(new BufferedInputStream(socket.getInputStream()));
      }
      catch (Exception e)
      {
         log.error("Cannot connect to the ConnectionReceiver/Server", e);
         throw new RemoteException("Cannot connect to the ConnectionReceiver/Server");
      }
   }

   /**
    * #Description of the Method
    *
    * @exception Exception  Description of Exception
    */
   private void waitAnswer()
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
            case OILConstants.EXCEPTION:
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
// vim:expandtab:tabstop=3:shiftwidth=3
