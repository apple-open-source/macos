/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.logging;

import java.net.Socket;
import java.net.ServerSocket;
import java.net.InetAddress;

import org.apache.log4j.LogManager;
import org.apache.log4j.MDC;
import org.apache.log4j.spi.LoggerRepository;
import org.apache.log4j.net.SocketNode;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.MissingAttributeException;

import org.jboss.logging.Logger;

/**
 * A Log4j SocketServer service.  Listens for client connections on the
 * specified port and creates a new thread and SocketNode to process the
 * incoming client log messages.
 *
 * <p>
 * The LoggerRepository can be changed based on the clients address
 * by using a custom LoggerRepositoryFactory.  The default factory
 * will simply return the current repository.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.2.2.1 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class Log4jSocketServer
   extends ServiceMBeanSupport
   implements Log4jSocketServerMBean
{
   /** The port number where the server listens. */
   protected int port = -1;

   /** The listen backlog count. */
   protected int backlog = 50;

   /** The address to bind to. */
   protected InetAddress bindAddress;
   
   /** True if the socket listener is enabled. */
   protected boolean listenerEnabled = true;

   /** The socket listener thread. */
   protected SocketListenerThread listenerThread;

   /** The server socket which the listener listens on. */
   protected ServerSocket serverSocket;

   /** The factory to create LoggerRepository's for client connections. */
   protected LoggerRepositoryFactory loggerRepositoryFactory;
   
   /**
    * @jmx:managed-constructor
    */
   public Log4jSocketServer()
   {
      super();
   }

   /**
    * @jmx:managed-attribute
    */
   public void setPort(final int port)
   {
      this.port = port;
   }

   /**
    * @jmx:managed-attribute
    */
   public int getPort()
   {
      return port;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setBacklog(final int backlog)
   {
      this.backlog = backlog;
   }

   /**
    * @jmx:managed-attribute
    */
   public int getBacklog()
   {
      return backlog;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setBindAddress(final InetAddress addr)
   {
      this.bindAddress = addr;
   }

   /**
    * @jmx:managed-attribute
    */
   public InetAddress getBindAddress()
   {
      return bindAddress;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setListenerEnabled(final boolean enabled)
   {
      listenerEnabled = enabled;
   }

   /**
    * @jmx:managed-attribute
    */
   public boolean setListenerEnabled()
   {
      return listenerEnabled;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setLoggerRepositoryFactoryType(final Class type)
      throws InstantiationException, IllegalAccessException, ClassCastException
   {
      this.loggerRepositoryFactory = (LoggerRepositoryFactory)type.newInstance();
   }

   /**
    * @jmx:managed-attribute
    */
   public Class getLoggerRepositoryFactoryType()
   {
      if (loggerRepositoryFactory == null)
         return null;
      return loggerRepositoryFactory.getClass();
   }

   /**
    * @jmx:managed-operation
    */
   public LoggerRepository getLoggerRepository(final InetAddress addr)
   {
      return loggerRepositoryFactory.create(addr);
   }
   

   ///////////////////////////////////////////////////////////////////////////
   //                             Socket Listener                           //
   ///////////////////////////////////////////////////////////////////////////

   protected class SocketListenerThread
      extends Thread
   {
      protected Logger log = Logger.getLogger(SocketListenerThread.class);
      protected boolean enabled;
      protected boolean shuttingDown;
      protected Object lock = new Object();
      
      public SocketListenerThread(final boolean enabled)
      {
         super("SocketListenerThread");

         this.enabled = enabled;
      }

      public void setEnabled(boolean enabled)
      {
         this.enabled = enabled;

         synchronized (lock)
         {
            lock.notifyAll();
         }

         if (log.isDebugEnabled())
         {
            log.debug("Notified that enabled: " + enabled);
         }
      }

      public void shutdown()
      {
         enabled = false;
         shuttingDown = true;

         synchronized (lock)
         {
            lock.notifyAll();
         }

         if (log.isDebugEnabled())
         {
            log.debug("Notified to shutdown");
         }
      }
      
      public void run()
      {
         while (!shuttingDown)
         {
            
            if (!enabled)
            {
               try
               {
                  log.debug("Disabled, waiting for notification");
                  synchronized (lock)
                  {
                     lock.wait();
                  }
               }
               catch (InterruptedException ignore)
               {
               }
            }

            try
            {
               doRun();
            }
            catch (Exception e)
            {
               log.error("Exception caught from main loop; ignoring", e);
            }
         }
      }

      protected void doRun() throws Exception
      {
         while (enabled)
         {
            Socket socket = serverSocket.accept();
            InetAddress addr =  socket.getInetAddress();
            log.debug("Connected to client at " + addr); 

            LoggerRepository repo = getLoggerRepository(addr);
            log.debug("Using repository: " + repo);

            //
            // jason: may want to expose socket node as an MBean for management
            //
            
            log.debug("Starting new socket node");
            SocketNode node = new SocketNode(socket, repo);
            /* Create a thread with and MDC.host value set to the client
            hostname to allow for distiguished output
            */
            String clientHost = addr.getHostName();
            SocketThread thread = new SocketThread(node, clientHost);
            thread.start();
            log.debug("Socket node started");
         }
      }
   }

   static class SocketThread
      extends Thread
   {
      String host;

      SocketThread(Runnable target, String host)
      {
         super(target, host+" LoggingEvent Thread");
         this.host = host;
      }
      public void run()
      {
         MDC.put("host", host);
         super.run();
      }
   }

   ///////////////////////////////////////////////////////////////////////////
   //                         LoggerRepositoryFactory                       //
   ///////////////////////////////////////////////////////////////////////////

   public static interface LoggerRepositoryFactory
   {
      public LoggerRepository create(InetAddress addr);
   }

   /**
    * A simple LoggerRepository factory which simply returns
    * the current repository from the LogManager.
    */
   public static class DefaultLoggerRepositoryFactory
      implements LoggerRepositoryFactory
   {
      private LoggerRepository repo;
      
      public LoggerRepository create(final InetAddress addr)
      {
         if (repo == null)
            repo = LogManager.getLoggerRepository();
         return repo;
      }
   }
   
   ///////////////////////////////////////////////////////////////////////////
   //                            Service Overrides                          //
   ///////////////////////////////////////////////////////////////////////////

   protected void createService() throws Exception
   {
      listenerThread = new SocketListenerThread(false);
      listenerThread.setDaemon(true);
      listenerThread.start();
      log.debug("Socket listener thread started");

      if (loggerRepositoryFactory == null)
      {
         log.debug("Using default logger repository factory");
         loggerRepositoryFactory = new DefaultLoggerRepositoryFactory();
      }
   }
      
   protected void startService() throws Exception
   {
      if (port == -1)
         throw new MissingAttributeException("Port");

      // create a new server socket to handle port number changes
      if (bindAddress == null)
      {
         serverSocket = new ServerSocket(port, backlog);
      }
      else
      {
         serverSocket = new ServerSocket(port, backlog, bindAddress);
      }

      log.info("Listening on " + serverSocket);
      
      listenerThread.setEnabled(listenerEnabled);
   }

   protected void stopService() throws Exception
   {
      listenerThread.setEnabled(false);
   }

   protected void destroyService() throws Exception
   {
      listenerThread.shutdown();
      listenerThread = null;
      serverSocket = null;
   }
}
