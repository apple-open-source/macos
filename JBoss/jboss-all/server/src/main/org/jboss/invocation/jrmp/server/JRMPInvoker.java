/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.invocation.jrmp.server;

import java.lang.reflect.Method;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.io.Serializable;
import java.rmi.server.RemoteServer;
import java.rmi.server.UnicastRemoteObject;
import java.rmi.server.RMIServerSocketFactory;
import java.rmi.server.RMIClientSocketFactory;
import java.rmi.server.RemoteStub;
import java.rmi.MarshalledObject;

import javax.management.ObjectName;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.naming.Name;
import javax.naming.InitialContext;
import javax.naming.Context;
import javax.naming.NamingException;
import javax.naming.NameNotFoundException;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.jboss.invocation.jrmp.interfaces.JRMPInvokerProxy;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.Invoker;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.invocation.MarshalledValueInputStream;
import org.jboss.logging.Logger;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.net.sockets.DefaultSocketFactory;
import org.jboss.proxy.TransactionInterceptor;
import org.jboss.security.SecurityDomain;
import org.jboss.system.Registry;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.tm.TransactionPropagationContextFactory;
import org.jboss.tm.TransactionPropagationContextImporter;
import org.jboss.util.ProfilerPoint;

/**
 * The JRMPInvoker is an RMI implementation that can generate Invocations
 * from RMI/JRMP into the JMX base.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 *
 * @author <a href="mailto:marc.fleury@jboss.org>Marc Fleury</a>
 * @author <a href="mailto:scott.stark@jboss.org>Scott Stark</a>
 * @version $Revision: 1.21.2.12 $
 */
public class JRMPInvoker
   extends RemoteServer
   implements Invoker, JRMPInvokerMBean, MBeanRegistration
{
   /** Identifer to instruct the usage of an anonymous port. */
   public static final int ANONYMOUS_PORT = 0;

   /** Instance logger. */
   protected Logger log;

   /** Service MBean support delegate. */
   protected ServiceMBeanSupport support;

   /** The port the container will be exported on */
   protected int rmiPort = ANONYMOUS_PORT;

   /** An optional custom client socket factory */
   protected RMIClientSocketFactory clientSocketFactory;

   /** An optional custom server socket factory */
   protected RMIServerSocketFactory serverSocketFactory;

   /** The class name of the optional custom client socket factory */
   protected String clientSocketFactoryName;

   /** The class name of the optional custom server socket factory */
   protected String serverSocketFactoryName;

   /** The address to bind the rmi port on */
   protected String serverAddress;
   /** The name of the security domain to use with server sockets that support SSL */
   protected String sslDomain;

   protected RemoteStub invokerStub;
   /** The socket accept backlog */
   protected int backlog = 200;
   /** A flag to enable caching of classes in the MarshalledValueInputStream */
   protected boolean enableClassCaching = false;

   private static TransactionPropagationContextFactory tpcFactory;
   private static TransactionPropagationContextImporter tpcImporter;

   public JRMPInvoker()
   {
      final JRMPInvoker delegate = this;

      // adapt the support delegate to invoke our state methods
      support = new ServiceMBeanSupport(getClass()) {

            protected void startService() throws Exception {
               delegate.startService();
            }

            protected void stopService() throws Exception {
               delegate.stopService();
            }

            protected void destroyService() throws Exception {
               delegate.destroyService();
            }
         };

      // Setup logging from delegate
      log = support.getLog();
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
   public void setBacklog(int back)
   {
      backlog = back;
   }

   /**
    * @jmx:managed-attribute
    */
   public boolean getEnableClassCaching()
   {
      return enableClassCaching;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setEnableClassCaching(boolean flag)
   {
      enableClassCaching = flag;
      MarshalledValueInputStream.useClassCache(enableClassCaching);
   }

   /**
    * @return The localhost name or null.
    */
   public String getServerHostName()
   {
      try
      {
         return InetAddress.getLocalHost().getHostName();
      }
      catch (Exception ignored)
      {
         return null;
      }
   }

   /**
    * @jmx:managed-attribute
    */
   public void setRMIObjectPort(final int rmiPort)
   {
      this.rmiPort = rmiPort;
   }

   /**
    * @jmx:managed-attribute
    */
   public int getRMIObjectPort()
   {
      return rmiPort;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setRMIClientSocketFactory(final String name)
   {
      clientSocketFactoryName = name;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getRMIClientSocketFactory()
   {
      return clientSocketFactoryName;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setRMIServerSocketFactory(final String name)
   {
      serverSocketFactoryName = name;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getRMIServerSocketFactory()
   {
      return serverSocketFactoryName;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setServerAddress(final String address)
   {
      serverAddress = address;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getServerAddress()
   {
      return serverAddress;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setSecurityDomain(String domainName)
   {
      this.sslDomain = domainName;
   }
   /**
    * @jmx:managed-attribute
    */
   public String getSecurityDomain()
   {
      return sslDomain;
   }

   public Serializable getStub()
   {
      return this.invokerStub;
   }

   protected void startService() throws Exception
   {
      loadCustomSocketFactories();

      if (log.isDebugEnabled())
      {
         log.debug("RMI Port='" +
                   (rmiPort == ANONYMOUS_PORT ? "Anonymous" :
                    Integer.toString(rmiPort))+"'");

         log.debug("Client SocketFactory='" +
                   (clientSocketFactory == null ? "Default" :
                    clientSocketFactory.toString())+"'");

         log.debug("Server SocketFactory='" +
                   (serverSocketFactory == null ? "Default" :
                    serverSocketFactory.toString())+"'");

         log.debug("Server SocketAddr='" +
                   (serverAddress == null ? "Default" :
                    serverAddress)+"'");
         log.debug("SecurityDomain='" +
                   (sslDomain == null ? "Default" :
                    sslDomain)+"'");
      }

      InitialContext ctx = new InitialContext();

      // Get the transaction propagation context factory
      tpcFactory = (TransactionPropagationContextFactory)
         ctx.lookup("java:/TransactionPropagationContextExporter");

      // and the transaction propagation context importer
      tpcImporter = (TransactionPropagationContextImporter)
         ctx.lookup("java:/TransactionPropagationContextImporter");

      // Set the transaction manager and transaction propagation
      // context factory of the GenericProxy class

      // FIXME marcf: This should not be here
      TransactionInterceptor.setTransactionManager((TransactionManager)ctx.lookup("java:/TransactionManager"));
      JRMPInvokerProxy.setTPCFactory(tpcFactory);

      Invoker delegateInvoker = createDelegateInvoker();

      // Make the remote invoker proxy available for use by the proxy factory
      Registry.bind(support.getServiceName(), delegateInvoker);

      // Export CI
      exportCI();

      // Bind the invoker in the JNDI invoker naming space
      rebind(
         // The context
         ctx,
         // It should look like so "invokers/<name>/jrmp"
         "invokers/" + InetAddress.getLocalHost().getHostName() + "/jrmp",
         // The bare invoker
         delegateInvoker);

      log.debug("Bound JRMP invoker for JMX node");

      ctx.close();
   }

   protected void stopService() throws Exception
   {
      InitialContext ctx = new InitialContext();

      try
      {
         unexportCI();
         ctx.unbind("invokers/"+InetAddress.getLocalHost().getHostName()+"/jrmp");
      }
      finally
      {
         ctx.close();
      }
      this.clientSocketFactory = null;
      this.serverSocketFactory = null;
      this.invokerStub = null;
   }

   protected void destroyService() throws Exception
   {
      // Export references to the bean
      Registry.unbind(support.getServiceName());
   }

   /**
    * Invoke a Remote interface method.
    */
   public Object invoke(Invocation invocation)
      throws Exception
   {
      /*
      ProfilerPoint.clear();
      ProfilerPoint.push("BEGIN JRMP");
      */
      Thread currentThread = Thread.currentThread();
      ClassLoader oldCl = currentThread.getContextClassLoader();
      ObjectName mbean = null;
      try
      {
         // Deserialize the transaction if it is there
         MarshalledInvocation mi = (MarshalledInvocation) invocation;
         invocation.setTransaction(importTPC(mi.getTransactionPropagationContext()));

         mbean = (ObjectName) Registry.lookup(invocation.getObjectName());

         // The cl on the thread should be set in another interceptor
         Object obj = support.getServer().invoke(mbean,
                                                 "invoke",
                                                 new Object[] {invocation},
                                                 Invocation.INVOKE_SIGNATURE);
         return new MarshalledObject(obj);
      }
      catch (Exception e)
      {
         e = (Exception) JMXExceptionDecoder.decode(e);
         if( log.isTraceEnabled() )
            log.trace("Failed to invoke on mbean: "+mbean, e);
         throw e;
      }
      finally
      {
         currentThread.setContextClassLoader(oldCl);
         Thread.interrupted(); // clear interruption because this thread may be pooled.
      }
   }

   protected Invoker createDelegateInvoker()
   {
      return new JRMPInvokerProxy(this);
   }

   protected void exportCI() throws Exception
   {
      this.invokerStub = (RemoteStub)UnicastRemoteObject.exportObject
         (this, rmiPort, clientSocketFactory, serverSocketFactory);
   }

   protected void unexportCI() throws Exception
   {
      UnicastRemoteObject.unexportObject(this, true);
   }

   protected void rebind(Context ctx, String name, Object val)
      throws NamingException
   {
      // Bind val to name in ctx, and make sure that all
      // intermediate contexts exist

      Name n = ctx.getNameParser("").parse(name);
      while (n.size() > 1)
      {
         String ctxName = n.get(0);
         try
         {
            ctx = (Context)ctx.lookup(ctxName);
         }
         catch (NameNotFoundException e)
         {
            ctx = ctx.createSubcontext(ctxName);
         }
         n = n.getSuffix(1);
      }

      ctx.rebind(n.get(0), val);
   }

   /** Load and instantiate the clientSocketFactory, serverSocketFactory using
    the TCL and set the bind address and SSL domain if the serverSocketFactory
    supports it.
   */
   protected void loadCustomSocketFactories()
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();

      try
      {
         if( clientSocketFactoryName != null )
         {
            Class csfClass = loader.loadClass(clientSocketFactoryName);
            clientSocketFactory = (RMIClientSocketFactory) csfClass.newInstance();
         }
      }
      catch (Exception e)
      {
         log.error("Failed to load client socket factory", e);
         clientSocketFactory = null;
      }

      try
      {
         if( serverSocketFactoryName != null )
         {
            Class ssfClass = loader.loadClass(serverSocketFactoryName);
            serverSocketFactory = (RMIServerSocketFactory) ssfClass.newInstance();
            if( serverAddress != null )
            {
               // See if the server socket supports setBindAddress(String)
               try
               {
                  Class[] parameterTypes = {String.class};
                  Method m = ssfClass.getMethod("setBindAddress", parameterTypes);
                  Object[] args = {serverAddress};
                  m.invoke(serverSocketFactory, args);
               }
               catch (NoSuchMethodException e)
               {
                  log.warn("Socket factory does not support setBindAddress(String)");
                  // Go with default address
               }
               catch (Exception e)
               {
                  log.warn("Failed to setBindAddress="+serverAddress+" on socket factory", e);
                  // Go with default address
               }
            }
            /* See if the server socket supports setSecurityDomain(SecurityDomain)
            if an sslDomain was specified
            */
            if( sslDomain != null )
            {
               try
               {
                  InitialContext ctx = new InitialContext();
                  SecurityDomain domain = (SecurityDomain) ctx.lookup(sslDomain);
                  Class[] parameterTypes = {SecurityDomain.class};
                  Method m = ssfClass.getMethod("setSecurityDomain", parameterTypes);
                  Object[] args = {domain};
                  m.invoke(serverSocketFactory, args);
               }
               catch(NoSuchMethodException e)
               {
                  log.error("Socket factory does not support setSecurityDomain(SecurityDomain)");
               }
               catch(Exception e)
               {
                  log.error("Failed to setSecurityDomain="+sslDomain+" on socket factory");
               }
            }
         }
         // If a bind address was specified create a DefaultSocketFactory
         else if( serverAddress != null )
         {
            DefaultSocketFactory defaultFactory = new DefaultSocketFactory(backlog);
            serverSocketFactory = defaultFactory;
            try
            {
               defaultFactory.setBindAddress(serverAddress);
            }
            catch (UnknownHostException e)
            {
               log.error("Failed to setBindAddress="+serverAddress+" on socket factory", e);
            }
         }
      }
      catch (Exception e)
      {
         log.error("operation failed", e);
         serverSocketFactory = null;
      }
   }

   /**
    * Import a transaction propagation context into the local VM, and
    * return the corresponding <code>Transaction</code>.
    *
    * @return A transaction or null if no tpc.
    */
   protected Transaction importTPC(Object tpc)
   {
      if (tpc != null)
         return tpcImporter.importTransactionPropagationContext(tpc);
      return null;
   }

   //
   // Delegate the ServiceMBean details to our support delegate
   //

   public String getName()
   {
      return support.getName();
   }

   public MBeanServer getServer()
   {
      return support.getServer();
   }

   public int getState()
   {
      return support.getState();
   }

   public String getStateString()
   {
      return support.getStateString();
   }

   public void create() throws Exception
   {
      support.create();
   }

   public void start() throws Exception
   {
      support.start();
   }

   public void stop()
   {
      support.stop();
   }

   public void destroy()
   {
      support.destroy();
   }

   public ObjectName preRegister(MBeanServer server, ObjectName name)
      throws Exception
   {
      return support.preRegister(server, name);
   }

   public void postRegister(Boolean registrationDone)
   {
      support.postRegister(registrationDone);
   }

   public void preDeregister() throws Exception
   {
      support.preDeregister();
   }

   public void postDeregister()
   {
      support.postDeregister();
   }
}
