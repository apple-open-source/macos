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

import java.net.Socket;
import java.rmi.RemoteException;

import javax.jms.Destination;
import javax.jms.JMSException;
import org.jboss.mq.Connection;
import org.jboss.mq.ReceiveRequest;

import org.jboss.logging.Logger;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.ClientILService;

/**
 * The RMI implementation of the ConnectionReceiver object
 *
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @version   $Revision: 1.6.4.2 $
 * @created   August 16, 2001
 */
public class UILClientILService implements Runnable, org.jboss.mq.il.ClientILService
{

   static Logger log = Logger.getLogger(UILClientILService.class);
   // Attributes ----------------------------------------------------
   final static int m_close = 2;
   final static int m_deleteTemporaryDestination = 1;
   final static int m_receive = 3;
   final static int m_pong = 4;
   //the client IL
   UILClientIL clientIL;
   //The thread that is doing the Socket reading work
   Thread worker;
   //A link on my connection
   private Connection connection;
   //Should this service be running ?
   private boolean running;   

   /**
    * getClientIL method comment.
    *
    * @return                         The ClientIL value
    * @exception java.lang.Exception  Description of Exception
    */
   public org.jboss.mq.il.ClientIL getClientIL()
          throws java.lang.Exception
   {
      return clientIL;
   }

   /**
    * init method comment.
    *
    * @param connection               Description of Parameter
    * @param props                    Description of Parameter
    * @exception java.lang.Exception  Description of Exception
    */
   public void init(org.jboss.mq.Connection connection, java.util.Properties props)
          throws java.lang.Exception
   {
      this.connection = connection;
      clientIL = new UILClientIL();
   }

   /**
    * Main processing method for the UILClientILService object
    */
   public void run()
   {
      boolean trace = log.isTraceEnabled();
      log.debug("UILClientILService.run()");

      Socket socket = null;
      int code = 0;

      ObjectOutputStream out = null;
      ObjectInputStream in = null;

      try
      {
         log.trace("getting streams");
         UILServerIL serverIL = (UILServerIL)connection.getServerIL();

         out = new ObjectOutputStream(new BufferedOutputStream(serverIL.mSocket.getOutputStream(2)));
         out.flush();
         in = new ObjectInputStream(new BufferedInputStream(serverIL.mSocket.getInputStream(2)));
         socket = serverIL.socket;
      }
      catch (IOException e)
      {
         log.trace("Could not initialize the UILClientIL Service", e);
         connection.asynchFailure("Could not initialize the UILClientIL Service.", e);
         running = false;
         return;
      }

      int count = 0;
      while (running)
      {
         try
         {
            if( trace )
               log.trace("Waiting for a messgage from the server");
            code = in.readByte();
            count ++;
         }
         catch (java.io.InterruptedIOException e)
         {
            continue;
         }
         catch (IOException e)
         {
            if( trace )
               log.trace("Exiting run loop on IOE", e);
            break;
         }

         try
         {
            boolean sendAck = true;

            if( trace )
               log.trace("Begin("+count+") Code: "+code);
            switch (code)
            {
               case m_receive:
                  int numReceives = in.readInt();
                  org.jboss.mq.ReceiveRequest[] messages = new org.jboss.mq.ReceiveRequest[numReceives];
                  for (int i = 0; i < numReceives; ++i)
                  {
                     messages[i] = new ReceiveRequest();
                     messages[i].readExternal(in);
                  }
                  connection.asynchDeliver(messages);
                  break;
               case m_deleteTemporaryDestination:
                  connection.asynchDeleteTemporaryDestination((SpyDestination)in.readObject());
                  break;
               case m_close:
                  connection.asynchClose();
                  break;
               case m_pong:
                  sendAck = false;
                  long pingTime = in.readLong();
                  connection.asynchPong(pingTime);
                  break;
               default:
                  RemoteException e = new RemoteException("UILClientIL invocation contained a bad method code.");
                  connection.asynchFailure("UILClientIL invocation contained a bad method code.", e);
                  throw e;
            }

            // Do not send an ack for waitAnswer unless sendAck is true
            if( sendAck == false )
               continue;

            //Everthing was OK
            try
            {
               out.writeByte(0);
               out.flush();
            }
            catch (IOException e)
            {
               if (running)
               {
                  break;
               }

               connection.asynchFailure("Connection failure", e);
               break;
            }
         }
         catch (Exception e)
         {
            if (running)
            {
               break;
            }

            try
            {
               out.writeByte(1);
               out.writeObject(e);
               out.reset();
               out.flush();
            }
            catch (IOException e2)
            {
               connection.asynchFailure("Connection failure", e2);
               break;
            }
         }
         if( trace )
            log.trace("End("+count+") Code: "+code);
      }

      running = false;
      try
      {
         out.close();
         in.close();
         socket.close();
      }
      catch (IOException e)
      {
         connection.asynchFailure("Error whle closing UILClientIL connection", e);
         return;
      }
   }

   /**
    * start method comment.
    *
    * @exception java.lang.Exception  Description of Exception
    */
   public void start()
          throws java.lang.Exception
   {

      running = true;
      worker = new Thread(connection.threadGroup, this, "UILClientILService");
      worker.setDaemon(true);
      worker.start();

   }

   /**
    * @exception java.lang.Exception  Description of Exception
    */
   public void stop()
          throws java.lang.Exception
   {
      running = false;
      worker.interrupt();
   }
}
