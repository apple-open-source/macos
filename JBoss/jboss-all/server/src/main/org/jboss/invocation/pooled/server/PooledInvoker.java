/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.invocation.pooled.server;

import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.LinkedList;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.pooled.interfaces.PooledMarshalledInvocation;
import org.jboss.invocation.pooled.interfaces.PooledInvokerProxy;
import org.jboss.invocation.pooled.interfaces.ServerAddress;
import org.jboss.invocation.jrmp.interfaces.JRMPInvokerProxy;
import org.jboss.logging.Logger;
import org.jboss.naming.Util;
import org.jboss.proxy.TransactionInterceptor;
import org.jboss.system.Registry;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.tm.TransactionPropagationContextFactory;
import org.jboss.tm.TransactionPropagationContextImporter;

/**
 * This invoker pools Threads and client connections to one server socket.
 * The purpose is to avoid a bunch of failings of RMI.
 * 
 * 1. Avoid making a client socket connection with every invocation call.
 *    This is very expensive.  Also on windows if too many clients try 
 *    to connect at the same time, you get connection refused exceptions.
 *    This invoker/proxy combo alleviates this.
 *
 * 2. Avoid creating a thread per invocation.  The client/server connection
 *    is preserved and attached to the same thread.

 * So we have connection pooling on the server and client side, and thread pooling
 * on the server side.  Pool, is an LRU pool, so resources should be cleaned up.
 * 
 *
 * @author    <a href="mailto:bill@jboss.org">Bill Burke</a>
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 */
public class PooledInvoker extends ServiceMBeanSupport
   implements PooledInvokerMBean, Runnable
{

   /**
    * logger instance.
    */
   final static protected Logger log = Logger.getLogger(PooledInvoker.class);

   /**
    * If the TcpNoDelay option should be used on the socket.
    */
   protected boolean enableTcpNoDelay = false;

   /**
    * The internet address to bind to by default.
    */
   protected String serverBindAddress = null;

   /**
    * The server port to bind to.
    */
   protected int serverBindPort = 0;

   /**
    * The internet address client will use to connect to the sever.
    */
   protected String clientConnectAddress = null;

   /**
    * The port a client will use to connect to the sever.
    */
   protected int clientConnectPort = 0;

   protected int backlog = 200;

   protected ServerSocket serverSocket = null;

   protected int timeout = 60000; // 60 seconds.

   protected int maxPoolSize = 300;

   protected int clientMaxPoolSize = 300;

   protected int numAcceptThreads = 1;
   protected Thread[] acceptThreads;

   protected LRUPool clientpool;
   protected LinkedList threadpool;
   protected boolean running = true;
   /**
    * ObjectName of the <code>transactionManagerService</code> we use.
    * Probably should not be here -- used to set txInterceptor tx mananger.
    */
   protected ObjectName transactionManagerService;

   protected PooledInvokerProxy optimizedInvokerProxy = null;

   protected static TransactionPropagationContextFactory tpcFactory;
   protected static TransactionPropagationContextImporter tpcImporter;

   ////////////////////////////////////////////////////////////////////////
   //
   // The following methods Override the ServiceMBeanSupport base class
   //
   ////////////////////////////////////////////////////////////////////////
   /**
    * Gives this JMX service a name.
    * @return   The Name value
    */
   public String getName()
   {
      return "Optimized-Invoker";
   }


   protected void jmxBind()
   {
      Registry.bind(getServiceName(), optimizedInvokerProxy);
   }

   /**
    * Starts this IL, and binds it to JNDI
    *
    * @exception Exception  Description of Exception
    */
   public void startService() throws Exception
   {

      ///////////////////////////////////////////////////////////      
      // Setup the transaction stuff
      ///////////////////////////////////////////////////////////      
      InitialContext ctx = new InitialContext();

      // Get the transaction propagation context factory
      tpcFactory = (TransactionPropagationContextFactory) ctx.lookup("java:/TransactionPropagationContextExporter");

      // and the transaction propagation context importer
      tpcImporter = (TransactionPropagationContextImporter) ctx.lookup("java:/TransactionPropagationContextImporter");

      // FIXME marcf: This should not be here
      TransactionInterceptor.setTransactionManager((TransactionManager)ctx.lookup("java:/TransactionManager"));

      //TransactionInterceptor.setTransactionManager((TransactionManager) ctx.lookup("java:/TransactionManager"));
      JRMPInvokerProxy.setTPCFactory(tpcFactory);

      ///////////////////////////////////////////////////////////      
      // Setup the socket level stuff
      ///////////////////////////////////////////////////////////      

      InetAddress bindAddress =
         (serverBindAddress == null || serverBindAddress.length() == 0)
            ? null
            : InetAddress.getByName(serverBindAddress);

      clientConnectAddress =
         (clientConnectAddress == null || clientConnectAddress.length() == 0)
            ? InetAddress.getLocalHost().getHostName()
            : clientConnectAddress;


      clientpool = new LRUPool(2, maxPoolSize);
      clientpool.create();
      threadpool = new LinkedList();
      serverSocket = new ServerSocket(serverBindPort, backlog, bindAddress);
      clientConnectPort = (clientConnectPort == 0) ? serverSocket.getLocalPort() : clientConnectPort;

      ServerAddress sa = new ServerAddress(clientConnectAddress, clientConnectPort, enableTcpNoDelay, timeout); 
      optimizedInvokerProxy = new PooledInvokerProxy(sa, clientMaxPoolSize);

      ///////////////////////////////////////////////////////////      
      // Register the service with the rest of the JBoss Kernel
      ///////////////////////////////////////////////////////////      
      // Export references to the bean
      jmxBind();
      // Bind the invoker in the JNDI invoker naming space
      // It should look like so "invokers/<hostname>/trunk" 
      Util.rebind(ctx, "invokers/" + clientConnectAddress + "/pooled", optimizedInvokerProxy);
      
      log.debug("Bound invoker for JMX node");
      ctx.close();

      acceptThreads = new Thread[numAcceptThreads];
      for (int i = 0; i < numAcceptThreads; i++)
      {
         acceptThreads[i] = new Thread(this);
         acceptThreads[i].start();
      }
   }

   public void run()
   {
      
      while (running)
      {
         try
         {
            Socket socket = serverSocket.accept();
            //System.out.println("Thread accepted: " + Thread.currentThread());
            ServerThread thread = null;
            boolean newThread = false;
            
            while (thread == null)
            {
               synchronized(threadpool)
               {
                  if (threadpool.size() > 0)
                  {
                     thread = (ServerThread)threadpool.removeFirst();
                  }
               }
               if (thread == null)
               {
                  synchronized(clientpool)
                  {
                     if (clientpool.size() < maxPoolSize) 
                     {
                        thread = new ServerThread(socket, this, clientpool, threadpool, timeout);
                        newThread = true;
                     }
                     if (thread == null)
                     {
                        clientpool.evict();
                        log.debug("**** WAITING *****");
                        clientpool.wait();
                        log.debug("**** WOKE UP *****");
                     }
                  }
               }
            }
            synchronized(clientpool)
            {
               clientpool.insert(thread, thread);
            }
            
            if (newThread)
            {
               log.debug("**** ACQUIRED NEW *****");
               thread.start();
            }
            else
            {
               log.debug("**** ACQUIRED OLD *****");
               thread.wakeup(socket, timeout);
            }
         }
         catch (Exception ex)
         {
            if (running)
               log.error("Failed to accept socket connection", ex);
         }
      }
   }

   /**
    * Stops this service, and unbinds it from JNDI.
    */
   public void stopService() throws Exception
   {

      InitialContext ctx = new InitialContext();
      running = false;
      maxPoolSize = 0; // so ServerThreads don't reinsert themselves
      for (int i = 0; i < acceptThreads.length; i++)
      {
         try
         {
            acceptThreads[i].interrupt();
         }
         catch (Exception ignored){}
      }
      clientpool.flush();
      for (int i = 0; i < threadpool.size(); i++)
      {
         ServerThread thread = (ServerThread)threadpool.removeFirst();
         thread.shutdown();
      }

      try
      {
         ctx.unbind("invokers/" + clientConnectAddress + "/pooled");
      }
      finally
      {
         ctx.close();
      }
      try
      {
         serverSocket.close();
      }
      catch(Exception e)
      {         
      }
   }

   protected void destroyService() throws Exception
   {
      // Unexport references to the bean
      Registry.unbind(getServiceName());
   }

   /**
    * The ServerProtocol will use this method to service an invocation 
    * request.
    */
   public Object invoke(Invocation invocation) throws Exception
   {
      Thread currentThread = Thread.currentThread();
      ClassLoader oldCl = currentThread.getContextClassLoader();
      try
      {

         // Deserialize the transaction if it is there
         PooledMarshalledInvocation mi = (PooledMarshalledInvocation) invocation;
         invocation.setTransaction(importTPC(mi.getTransactionPropagationContext()));
         ObjectName mbean = (ObjectName) Registry.lookup(invocation.getObjectName());

         // The cl on the thread should be set in another interceptor
         Object obj = getServer().invoke(mbean, "invoke",
               new Object[] { invocation }, Invocation.INVOKE_SIGNATURE);

         return obj;
      }
      catch (Exception e)
      {
         org.jboss.mx.util.JMXExceptionDecoder.rethrow(e);

         // the compiler does not know an exception is thrown by the above
         throw new org.jboss.util.UnreachableStatementException();
      }
      finally
      {
         currentThread.setContextClassLoader(oldCl);
      }
   }

   protected Transaction importTPC(Object tpc)
   {
      if (tpc != null)
         return tpcImporter.importTransactionPropagationContext(tpc);
      return null;
   }

   //The following are the mbean attributes for TrunkInvoker

   /**
    * Getter for property numAcceptThreads
    *
    * @return Value of property numAcceptThreads
    * @jmx:managed-attribute
    */
   public int getNumAcceptThreads()
   {
      return numAcceptThreads;
   }

   /**
    * Setter for property numAcceptThreads
    *
    * @param serverBindPort New value of property numAcceptThreads.
    * @jmx:managed-attribute
    */
   public void setNumAcceptThreads(int size)
   {
      this.numAcceptThreads = size;
   }

   /**
    * Getter for property maxPoolSize;
    *
    * @return Value of property maxPoolSize.
    * @jmx:managed-attribute
    */
   public int getMaxPoolSize()
   {
      return maxPoolSize;
   }

   /**
    * Setter for property maxPoolSize.
    *
    * @param serverBindPort New value of property serverBindPort.
    * @jmx:managed-attribute
    */
   public void setMaxPoolSize(int maxPoolSize)
   {
      this.maxPoolSize = maxPoolSize;
   }

   /**
    * Getter for property maxPoolSize;
    *
    * @return Value of property maxPoolSize.
    * @jmx:managed-attribute
    */
   public int getClientMaxPoolSize()
   {
      return clientMaxPoolSize;
   }

   /**
    * Setter for property maxPoolSize.
    *
    * @param serverBindPort New value of property serverBindPort.
    * @jmx:managed-attribute
    */
   public void setClientMaxPoolSize(int clientMaxPoolSize)
   {
      this.clientMaxPoolSize = clientMaxPoolSize;
   }

   /**
    * Getter for property timeout
    *
    * @return Value of property timeout
    * @jmx:managed-attribute
    */
   public int getSocketTimeout()
   {
      return timeout;
   }

   /**
    * Setter for property timeout
    *
    * @param serverBindPort New value of property timeout
    * @jmx:managed-attribute
    */
   public void setSocketTimeout(int time)
   {
      this.timeout = time;
   }

   /**
    *
    * @return Value of property serverBindPort.
    * @jmx:managed-attribute
    */
   public int getCurrentClientPoolSize()
   {
      return clientpool.size();
   }

   /**
    *
    * @return Value of property serverBindPort.
    * @jmx:managed-attribute
    */
   public int getCurrentThreadPoolSize()
   {
      return threadpool.size();
   }

   /**
    * Getter for property serverBindPort.
    *
    * @return Value of property serverBindPort.
    * @jmx:managed-attribute
    */
   public int getServerBindPort()
   {
      return serverBindPort;
   }

   /**
    * Setter for property serverBindPort.
    *
    * @param serverBindPort New value of property serverBindPort.
    * @jmx:managed-attribute
    */
   public void setServerBindPort(int serverBindPort)
   {
      this.serverBindPort = serverBindPort;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getClientConnectAddress()
   {
      return clientConnectAddress;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setClientConnectAddress(String clientConnectAddress)
   {
      this.clientConnectAddress = clientConnectAddress;
   }

   /**
    * @jmx:managed-attribute
    */
   public int getClientConnectPort()
   {
      return clientConnectPort;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setClientConnectPort(int clientConnectPort)
   {
      this.clientConnectPort = clientConnectPort;
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
   public void setBacklog(int backlog)
   {
      this.backlog = backlog;
   }

   /**
    * @jmx:managed-attribute
    */
   public boolean isEnableTcpNoDelay()
   {
      return enableTcpNoDelay;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setEnableTcpNoDelay(boolean enableTcpNoDelay)
   {
      this.enableTcpNoDelay = enableTcpNoDelay;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getServerBindAddress()
   {
      return serverBindAddress;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setServerBindAddress(String serverBindAddress)
   {
      this.serverBindAddress = serverBindAddress;
   }

   
   
   /**
    * mbean get-set pair for field transactionManagerService
    * Get the value of transactionManagerService
    * @return value of transactionManagerService
    *
    * @jmx:managed-attribute
    */
   public ObjectName getTransactionManagerService()
   {
      return transactionManagerService;
   }
   
   
   /**
    * Set the value of transactionManagerService
    * @param transactionManagerService  Value to assign to transactionManagerService
    *
    * @jmx:managed-attribute
    */
   public void setTransactionManagerService(ObjectName transactionManagerService)
   {
      this.transactionManagerService = transactionManagerService;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public PooledInvokerProxy getOptimizedInvokerProxy()
   {
      return optimizedInvokerProxy;
   }

}
// vim:expandtab:tabstop=3:shiftwidth=3
