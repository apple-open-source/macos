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
import java.net.ServerSocket;

import java.net.Socket;
import java.rmi.RemoteException;

import javax.jms.Destination;
import javax.jms.JMSException;
import org.jboss.mq.Connection;
import org.jboss.mq.ReceiveRequest;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.ClientILService;

/**
 * The RMI implementation of the ConnectionReceiver object
 *
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @version   $Revision: 1.9 $
 * @created   August 16, 2001
 */
public final class OILClientILService
   implements java.lang.Runnable,
      org.jboss.mq.il.ClientILService
{
   private final static org.jboss.logging.Logger cat = org.jboss.logging.Logger.getLogger(OILClientILService.class);

   //the client IL
   private OILClientIL clientIL;

   //The thread that is doing the Socket reading work
   private Thread worker;

   // the connected client
   private Socket socket = null;

   //A link on my connection
   private Connection connection;

   //Should this service be running ?
   private boolean running;

   //The server socket we listen for a connection on
   private ServerSocket serverSocket;

   /**
    * Number of OIL Worker threads started.
    */
   private static int threadNumber= 0;

   /**
    * If the TcpNoDelay option should be used on the socket.
    */
   private boolean enableTcpNoDelay=false;

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
      serverSocket = new ServerSocket(0);

      String t = props.getProperty(OILServerILFactory.OIL_TCPNODELAY_KEY);
      if (t != null) 
         enableTcpNoDelay = t.equals("yes");

      clientIL = new OILClientIL(java.net.InetAddress.getLocalHost(), serverSocket.getLocalPort(), enableTcpNoDelay);
      
   }

   /**
    * Main processing method for the OILClientILService object
    */
   public void run()
   {
      int code = 0;
      ObjectOutputStream out = null;
      ObjectInputStream in = null;
      socket = null;
      int serverPort = serverSocket.getLocalPort();

      try
      {
         if( cat.isDebugEnabled() )
            cat.debug("Waiting for the server to connect to me on port " +serverSocket.getLocalPort());

         // We may close() before we get a connection so we need to
         // periodicaly check to see if we were !running.
         //
         serverSocket.setSoTimeout(1000);
         while (running && socket == null)
         {
            try
            {
               socket = serverSocket.accept();
            }
            catch (java.io.InterruptedIOException e)
            {
               // do nothing, running flag will be checked
               continue;
            }
            catch (IOException e)
            {
               if (running)
                  connection.asynchFailure("Error accepting connection from server in OILClientILService.", e);
               return; // finally block will clean up!
            }
         }

         if(running)
         {
            socket.setTcpNoDelay(enableTcpNoDelay);
            socket.setSoTimeout(0);
            out = new ObjectOutputStream(new BufferedOutputStream(socket.getOutputStream()));
            out.flush();
            in = new ObjectInputStream(new BufferedInputStream(socket.getInputStream()));
         }
         else
         {
            // not running so exit
            // let the finally block do the clean up
            //
            return;
         }
      }
      catch (IOException e)
      {
         connection.asynchFailure("Could not initialize the OILClientIL Service.", e);
         return;
      }
      finally
      {
         try
         {
            serverSocket.close();
            serverSocket = null;
         }
         catch (IOException e)
         {
            if(cat.isDebugEnabled())
               cat.debug("run: an error occured closing the server socket", e);
         }
      }

      // now process request from the client.
      //
      while (running)
      {
         try
         {
            code = in.readByte();
         }
         catch (java.io.InterruptedIOException e)
         {
            continue;
         }
         catch (IOException e)
         {
            // Server has gone, bye, bye
            break;
         }

         try
         {

            switch (code)
            {
               case OILConstants.RECEIVE:
                  int numReceives = in.readInt();
                  org.jboss.mq.ReceiveRequest[] messages = new org.jboss.mq.ReceiveRequest[numReceives];
                  for (int i = 0; i < numReceives; ++i)
                  {
                     messages[i] = new ReceiveRequest();
                     messages[i].readExternal(in);
                  }
                  connection.asynchDeliver(messages);
                  break;

               case OILConstants.DELETE_TEMPORARY_DESTINATION:
                  connection.asynchDeleteTemporaryDestination((SpyDestination)in.readObject());
                  break;

               case OILConstants.CLOSE:
                  connection.asynchClose();
                  break;

               case OILConstants.PONG:
                  connection.asynchPong(in.readLong());
                  break;

               default:
                  throw new RemoteException("Bad method code !");
            }

            //Everthing was OK
            //
            try
            {
               out.writeByte(OILConstants.SUCCESS);
               out.flush();
            }
            catch (IOException e)
            {
               connection.asynchFailure("Connection failure(1)", e);
               break; // exit the loop
            }
         }
         catch (Exception e)
         {
            if (!running)
            {
               // if not running then don't bother to log an error
               //
               break;
            }

            try
            {
               cat.error("Exception handling server request", e);
               out.writeByte(OILConstants.EXCEPTION);
               out.writeObject(e);
               out.reset();
               out.flush();
            }
            catch (IOException e2)
            {
               connection.asynchFailure("Connection failure(2)", e2);
               break;
            }
         }
      } // end while

      // exited loop, so clean up the conection
      //
      try
      {
         cat.debug("Closing receiver connections on port: " + serverPort);
         out.close();
         in.close();
         socket.close();
         socket = null;
      }
      catch (IOException e)
      {
         connection.asynchFailure("Connection failure", e);
      }

      // ensure the flag is set correctly
      //
      running = false;
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
      worker = new Thread(connection.threadGroup, this, "OILClientILService-" +threadNumber++);
      worker.setDaemon(true);
      worker.start();

   }

   /**
    * @exception java.lang.Exception  Description of Exception
    */
   public void stop()
          throws java.lang.Exception
   {
      cat.trace("Stop called on OILClientService");
      running = false;
      worker.interrupt();
   }
}
// vim:expandtab:tabstop=3:shiftwidth=3







