/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.connectionmanager;

import java.io.PrintWriter;
import java.io.Serializable;
import java.security.Principal;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.management.MBeanNotificationInfo;
import javax.management.Notification;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionEvent;
import javax.resource.spi.ConnectionManager;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;
import javax.transaction.SystemException;

import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.jboss.logging.util.LoggerWriter;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.mx.util.MBeanServerLocator;
import org.jboss.naming.NonSerializableFactory;
import org.jboss.resource.JBossResourceException;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.SubjectSecurityManager;
import org.jboss.system.ServiceMBeanSupport;

/**
 * The BaseConnectionManager2 is an abstract base class for JBoss ConnectionManager
 * implementations.  It includes functionality to obtain managed connections from
 * a ManagedConnectionPool mbean, find the Subject from a SubjectSecurityDomain,
 * and interact with the CachedConnectionManager for connections held over
 * transaction and method boundaries.  Important mbean references are to a
 * ManagedConnectionPool supplier (typically a JBossManagedConnectionPool), and a
 * RARDeployment representing the ManagedConnectionFactory.
 *
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:E.Guib@ceyoniq.com">Erwin Guib</a>
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.18.2.21 $
 * @jmx:mbean name="jboss.jca:service=BaseConnectionManager"
 *            extends="org.jboss.system.ServiceMBean"
 */
public abstract class BaseConnectionManager2
   extends ServiceMBeanSupport
   implements BaseConnectionManager2MBean, ConnectionCacheListener,
              ConnectionListenerFactory
{
   /**
    * Note that this copy has a trailing / unlike the original in
    * JaasSecurityManagerService.
    */
   private static final String SECURITY_MGR_PATH = "java:/jaas/";

   public static final String STOPPING_NOTIFICATION = "jboss.jca.connectionmanagerstopping";

   private ObjectName managedConnectionPoolName;

   protected ManagedConnectionPool poolingStrategy;

   protected String jndiName;

   //private ObjectName securityDomainName; //for the future??
   private String securityDomainJndiName;
   private SubjectSecurityManager /*SecurityDomain*/ securityDomain;

   private ObjectName jaasSecurityManagerService;


   private ObjectName ccmName;
   private CachedConnectionManager ccm;

   // JSR-77 Managed Object
   ObjectName jcaConnectionFactory;
   ObjectName jcaManagedConnectionFactory;

   protected boolean trace;

   /**
    * Rethrow a throwable as resource exception
    */
   protected static void rethrowAsResourceException(String message, Throwable t)
      throws ResourceException
   {
      if (t instanceof ResourceException)
         throw (ResourceException) t;
      throw new JBossResourceException(message, t);
   }

   /**
    * Default BaseConnectionManager2 managed constructor for use by subclass mbeans.
    */
   public BaseConnectionManager2()
   {
      super();
      trace = log.isTraceEnabled();
   }

   /**
    * Creates a new <code>BaseConnectionManager2</code> instance.
    * for TESTING ONLY! not a managed operation.
    * @param ccm a <code>CachedConnectionManager</code> value
    * @param poolingStrategy a <code>ManagedConnectionPool</code> value
    */
   public BaseConnectionManager2(CachedConnectionManager ccm,
                                 ManagedConnectionPool poolingStrategy)
   {
      super();
      this.ccm = ccm;
      this.poolingStrategy = poolingStrategy;
      trace = log.isTraceEnabled();
   }


   /**
    * For testing
    */
   public ManagedConnectionPool getPoolingStrategy()
   {
      return poolingStrategy;
   }

   /**
    * The JndiName attribute holds the jndi name the ConnectionFactory
    * will be bound under in jndi.  Note that an entry of the form DefaultDS2
    * will be bound to java:/DefaultDS2.
    *
    * @return the JndiName value.
    * @jmx:managed-attribute
    */
   public String getJndiName()
   {
      return jndiName;
   }

   /**
    * Set the JndiName value.
    * @param jndiName The JndiName value.
    * @jmx:managed-attribute
    */
   public void setJndiName(String jndiName)
   {
      this.jndiName = jndiName;
   }




   /**
    * The ManagedConnectionPool holds the ObjectName of the mbean representing
    * the pool for this connection manager.  Normally it will be an embedded
    * mbean in a depends tag rather than an ObjectName reference to the mbean.
    * @return the ManagedConnectionPool value.
    * @jmx:managed-attribute
    */
   public ObjectName getManagedConnectionPool()
   {
      return managedConnectionPoolName;
   }

   /**
    * Set the ManagedConnectionPool value.
    * @param newManagedConnectionPool The new ManagedConnectionPool value.
    * @jmx:managed-attribute
    */
   public void setManagedConnectionPool(ObjectName newManagedConnectionPool)
   {
      this.managedConnectionPoolName = newManagedConnectionPool;
   }

   /**
    * The CachecConnectionManager holds the ObjectName of the
    * CachedConnectionManager mbean used by this ConnectionManager.
    * Normally this will be a depends tag with the ObjectName of the
    * unique CachedConnectionManager for the server.
    *
    * @param ccmName an <code>ObjectName</code> value
    * @jmx:managed-attribute
    */
   public void setCachedConnectionManager(ObjectName ccmName)
   {
      this.ccmName = ccmName;
   }

   /**
    * Describe <code>getCachedConnectionManager</code> method here.
    *
    * @return an <code>ObjectName</code> value
    * @jmx:managed-attribute
    */
   public ObjectName getCachedConnectionManager()
   {
      return ccmName;
   }

   /**
    *  The SecurityDomainJndiName holds the jndi name of the security domain
    * configured for the ManagedConnectionFactory this ConnectionManager
    * manages.  It is normally of the form java:/jaas/firebirdRealm,
    * where firebirdRealm is the name found in auth.conf or equivalent file.
    *
    * @param name an <code>String</code> value
    * @jmx:managed-attribute
    */
   public void setSecurityDomainJndiName(String securityDomainJndiName)
   {
      if (securityDomainJndiName != null
          && securityDomainJndiName.startsWith(SECURITY_MGR_PATH))
      {
         securityDomainJndiName = securityDomainJndiName.substring(SECURITY_MGR_PATH.length());
         log.warn("WARNING: UPDATE YOUR SecurityDomainJndiName! REMOVE " + SECURITY_MGR_PATH);
      } // end of if ()
      this.securityDomainJndiName = securityDomainJndiName;
   }

   /**
    * Get the SecurityDomainJndiName value.
    * @return the SecurityDomainJndiName value.
    * @jmx:managed-attribute
    */
   public String getSecurityDomainJndiName()
   {
      return securityDomainJndiName;
   }

   /**
    * Get the JaasSecurityManagerService value.
    * @return the JaasSecurityManagerService value.
    * @jmx:managed-attribute
    */
   public ObjectName getJaasSecurityManagerService()
   {
      return jaasSecurityManagerService;
   }

   /**
    * Set the JaasSecurityManagerService value.
    * @param newJaasSecurityManagerService The new JaasSecurityManagerService value.
    * @jmx:managed-attribute
    */
   public void setJaasSecurityManagerService(final ObjectName jaasSecurityManagerService)
   {
      this.jaasSecurityManagerService = jaasSecurityManagerService;
   }

   /**
    * ManagedConnectionFactory is an internal attribute that holds the
    * ManagedConnectionFactory instance managed by this ConnectionManager.
    *
    * @return value of managedConnectionFactory
    *
    * @jmx.managed-attribute access="read-only"
    */
   public ManagedConnectionFactory getManagedConnectionFactory()
   {
      return poolingStrategy.getManagedConnectionFactory();
   }

   /**
    * Describe <code>getInstance</code> method here.
    *
    * @return a <code>BaseConnectionManager2</code> value
    *
    * @jmx.managed-attribute access="read-only"
    */
   public BaseConnectionManager2 getInstance()
   {
      return this;
   }

   //ServiceMBeanSupport

   protected void startService() throws Exception
   {
      try
      {
         ccm = (CachedConnectionManager)server.getAttribute(ccmName, "Instance");
      }
      catch (Exception e)
      {
         JMXExceptionDecoder.rethrow(e);
      } // end of try-catch

      if (ccm == null)
      {
         throw new DeploymentException("cached ConnectionManager not found: " + ccmName);
      } // end of if ()

      if (securityDomainJndiName != null && jaasSecurityManagerService == null)
      {
         throw new DeploymentException("You must supply both securityDomainJndiName and jaasSecurityManagerService to use container managed security");
      } // end of if ()


      if (securityDomainJndiName != null)
      {
         securityDomain = (SubjectSecurityManager)new InitialContext().lookup(SECURITY_MGR_PATH + securityDomainJndiName);
      } // end of if ()

      if (managedConnectionPoolName == null)
      {
         throw new DeploymentException("managedConnectionPool not set!");
      } // end of if ()
      try
      {
         poolingStrategy = (ManagedConnectionPool)server.getAttribute(
            managedConnectionPoolName,
            "ManagedConnectionPool");
      }
      catch (Exception e)
      {
         JMXExceptionDecoder.rethrow(e);
      } // end of try-catch

      poolingStrategy.setConnectionListenerFactory(this);

      // Give it somewhere to tell people things
      String categoryName = poolingStrategy.getManagedConnectionFactory().getClass().getName() + "." + jndiName;
      Logger log = Logger.getLogger(categoryName);
      PrintWriter logWriter = new LoggerWriter(((org.jboss.logging.Log4jLoggerPlugin)log.getLoggerPlugin ()).getLogger());
      try
      {
         poolingStrategy.getManagedConnectionFactory().setLogWriter(logWriter);
      }
      catch (ResourceException re)
      {
         log.warn("Unable to set log writer '" + logWriter + "' on " +
                  "managed connection factory", re);
         log.warn("Linked exception:", re.getLinkedException());
      }

      //bind into jndi
      String bindName = "java:/" + jndiName;
      try
      {

         Object cf = poolingStrategy.getManagedConnectionFactory().createConnectionFactory(new ConnectionManagerProxy(this, this.serviceName));
         if (log.isDebugEnabled())
            log.debug("Binding object '" + cf + "' into JNDI at '" + bindName + "'");
         Name name = new InitialContext().getNameParser("").parse(bindName);
         NonSerializableFactory.rebind(name, cf, true);
         log.info("Bound connection factory for resource adapter for ConnectionManager '" + serviceName + " to JNDI name '" + bindName + "'");
      }
      catch (ResourceException re)
      {
         log.error("Could not create ConnectionFactory for resource adapter for ConnectionManager: " + serviceName, re);
         throw new DeploymentException("Could not create ConnectionFactory for adapter: " + poolingStrategy.getManagedConnectionFactory().getClass());
      } // end of try-catch
      catch (NamingException ne)
      {
         log.error("Unable to bind connection factory to JNDI name '" +
                   bindName + "'", ne);
         throw new DeploymentException("Could not bind ConnectionFactory into jndi: " + bindName);
      }
   }

   protected void stopService()
      throws Exception
   {
      //notify the login modules the mcf is going away, they need to look it up again later.
      sendNotification(new Notification(STOPPING_NOTIFICATION,
                                        getServiceName(),
                                        getNextNotificationSequenceNumber()));
      if (jaasSecurityManagerService != null && securityDomainJndiName != null)
      {
         server.invoke(jaasSecurityManagerService,
                       "flushAuthenticationCache",
                       new Object[] {securityDomainJndiName},
                       new String[] {String.class.getName()});

      } // end of if ()

      String bindName = "java:/" + jndiName;
      try
      {
         new InitialContext().unbind(bindName);
         NonSerializableFactory.unbind(bindName);
      }
      catch (NamingException ne)
      {
         log.error("could not unbind managedConnectionFactory from jndi: " + jndiName, ne);
      } // end of try-catch

      poolingStrategy.setConnectionListenerFactory(null);

      poolingStrategy = null;
      securityDomain = null;
      ccm = null;
   }

   /**
    * Describe <code>getManagedConnection</code> method here.
    * Public for use in testing pooling functionality by itself.
    * called by both allocateConnection and reconnect.
    * @param subject a <code>Subject</code> value
    * @param cri a <code>ConnectionRequestInfo</code> value
    * @return a <code>ManagedConnection</code> value
    * @exception ResourceException if an error occurs
    */
   public ConnectionListener getManagedConnection(Subject subject, ConnectionRequestInfo cri)
      throws ResourceException
   {
      return poolingStrategy.getConnection(subject, cri);
   }

   public void returnManagedConnection(ConnectionListener cl, boolean kill)
   {
      ManagedConnectionPool localStrategy = cl.getManagedConnectionPool();
      if (localStrategy != poolingStrategy)
         kill = true;

      try
      {
         localStrategy.returnConnection(cl, kill);
      }
      catch (ResourceException re)
      {
         // We can receive notification of an error on the connection
         // before it has been assigned to the pool. Reduce the noise for
         // these errors
         if (kill)
            log.debug("resourceException killing connection (error retrieving from pool?)", re);
         else
            log.warn("resourceException returning connection: " + cl.getManagedConnection(), re);
      } // end of try-catch
   }

   public int getConnectionCount()
   {
      return poolingStrategy.getConnectionCount();
   }

   // implementation of javax.resource.spi.ConnectionManager interface

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public Object allocateConnection(ManagedConnectionFactory mcf,
                                    ConnectionRequestInfo cri)
      throws ResourceException
   {
      if (poolingStrategy == null)
         throw new ResourceException("You are trying to use a connection factory that has been shut down: ManagedConnectionFactory is null.");

      //it is an explicit spec requirement that equals be used for matching rather than ==.
      if (!poolingStrategy.getManagedConnectionFactory().equals(mcf))
         throw new ResourceException("Wrong ManagedConnectionFactory sent to allocateConnection!");

      // Pick a managed connection from the pool
      Subject subject = getSubject();
      ConnectionListener cl = getManagedConnection(subject, cri);

      // Tell each connection manager the managed connection is active
      try
      {
         //WRONG METHOD NAME!!
         managedConnectionReconnected(cl);
      }
      catch (Throwable t)
      {
         managedConnectionDisconnected(cl);
         rethrowAsResourceException("Unchecked throwable in managedConnectionReconnected()", t);
      }

      // Ask the managed connection for a connection
      Object connection = null;
      try
      {
         connection = cl.getManagedConnection().getConnection(subject, cri);
      }
      catch (Throwable t)
      {
         managedConnectionDisconnected(cl);
         rethrowAsResourceException("Unchecked throwable in ManagedConnection.getConnection()", t);
      }

      // Associate managed connection with the connection
      registerAssociation(cl, connection);
      if (ccm != null)
          ccm.registerConnection(this, cl, connection, cri);
      return connection;
   }

   //ConnectionCacheListener implementation
   public void transactionStarted(Collection conns) throws SystemException
   {
      //reimplement in subclasses
   }

   /**
    * Describe <code>reconnect</code> method here.
    *
    * @param conns a <code>Collection</code> value
    * @exception ResourceException if an error occurs
    * @todo decide if the warning situation should throw an exception
    */
   public void reconnect(Collection conns, Set unsharableResources) throws ResourceException
   {
      // if we have an unshareable connection the association was not removed
      // nothing to do
      if(unsharableResources.contains(jndiName))
      {
         log.trace("reconnect for unshareable connection: nothing to do");
         return;
      }

      Map criToCLMap = new HashMap();
      for (Iterator i = conns.iterator(); i.hasNext(); )
      {
         ConnectionRecord cr = (ConnectionRecord)i.next();
         if (cr.cl != null)
         {
            //This might well be an error.
            log.warn("reconnecting a connection handle that still has a managedConnection! " + cr.cl.getManagedConnection() + " " + cr.connection);
         }
         ConnectionListener cl = (ConnectionListener)criToCLMap.get(cr.cri);
         if (cl == null)
         {
            cl = getManagedConnection(getSubject(), cr.cri);
            criToCLMap.put(cr.cri, cl);
            //only call once per managed connection, when we get it.
            managedConnectionReconnected(cl);
         } // end of if ()

         cl.getManagedConnection().associateConnection(cr.connection);
         registerAssociation(cl, cr.connection);
         cr.setConnectionListener(cl);
      } // end of for ()
      criToCLMap.clear();//not needed logically, might help the gc.
   }


   public void disconnect(Collection crs, Set unsharableResources) throws ResourceException
   {
      // if we have an unshareable connection do not remove the association
      // nothing to do
      if(unsharableResources.contains(jndiName))
      {
         log.trace("disconnect for unshareable connection: nothing to do");
         return;
      }

      Set cls = new HashSet();
      for (Iterator i = crs.iterator(); i.hasNext(); )
      {
         ConnectionRecord cr = (ConnectionRecord)i.next();
         ConnectionListener cl = cr.cl;
         cr.setConnectionListener(null);
         unregisterAssociation(cl, cr.connection);
         if (!cls.contains(cl))
         {
            cls.add(cl);
         } 
      }
      for (Iterator i = cls.iterator(); i.hasNext(); )
      {
         managedConnectionDisconnected((ConnectionListener)i.next());
      } // end of for ()

   }

   // implementation of javax.management.NotificationBroadcaster interface


   /**
    *
    * @return <description>
    */
   public MBeanNotificationInfo[] getNotificationInfo()
   {
      // TODO: implement this javax.management.NotificationBroadcaster method
      return super.getNotificationInfo();
   }

   //protected methods

   //does NOT put the mc back in the pool if no more handles. Doing so would introduce a race condition
   //whereby the mc got back in the pool while still enlisted in the tx.
   //The mc could be checked out again and used before the delist occured.
   protected void unregisterAssociation(ConnectionListener cl, Object c) throws ResourceException
   {
      cl.unregisterConnection(c);
   }

   protected final CachedConnectionManager getCcm()
   {
      return ccm;
   }

   //reimplement in subclasses to e.g. enlist in current tx.
   protected void managedConnectionReconnected(ConnectionListener cl) throws ResourceException
   {
   }

   //reimplement in subclasses to e.g. enlist in current tx.
   protected void managedConnectionDisconnected(ConnectionListener cl) throws ResourceException
   {
   }


   private void registerAssociation(ConnectionListener cl, Object c) throws ResourceException
   {
      cl.registerConnection(c);
   }

   private Subject getSubject()
   {
      Subject subject = null;
      if (securityDomain != null)
      {
         /* Authenticate using the caller info and obtain a copy of the Subject
            state for use in establishing a secure connection. A copy must be
            obtained to avoid problems with multiple threads associated with
            the same principal changing the state of the resulting Subject.
         */
         Principal principal = SecurityAssociation.getPrincipal();
         Object credential = SecurityAssociation.getCredential();
         subject = new Subject();
         if (securityDomain.isValid(principal, credential, subject) == false)
         {
            throw new SecurityException("Invalid authentication attempt, principal=" + principal);
         } // end of if
      } // end of if ()
      if (trace)
         log.trace("subject: " + subject);
      return subject;
   }

   // ConnectionListenerFactory

   public boolean isTransactional()
   {
      return false;
   }

   //ConnectionListener

   protected abstract class BaseConnectionEventListener implements ConnectionListener
   {

      private final ManagedConnection mc;

      private final ManagedConnectionPool mcp;
      
      private final Object context;

      private int state = NORMAL;

      private final List handles = new LinkedList();

      private long lastUse;

      protected Logger log;
      protected boolean trace;

      protected BaseConnectionEventListener(ManagedConnection mc, ManagedConnectionPool mcp, Object context, Logger log)
      {
         this.mc = mc;
         this.mcp = mcp;
         this.context = context;
         this.log = log;
         trace = log.isTraceEnabled();
         lastUse = System.currentTimeMillis();
      }

      public ManagedConnection getManagedConnection()
      {
         return mc;
      }

      public ManagedConnectionPool getManagedConnectionPool()
      {
         return mcp;
      }
      
      public Object getContext()
      {
         return context;
      }

      public int getState()
      {
         return state;
      }
      
      public void setState(int newState)
      {
         this.state = newState;
      }
      
      public boolean isTimedOut(long timeout)
      {
         return lastUse < timeout;
      }

      public void used()
      {
         lastUse = System.currentTimeMillis();
      }

      public synchronized void registerConnection(Object handle)
      {
         handles.add(handle);
      }

      public synchronized void unregisterConnection(Object handle)
      {
         if (!handles.remove(handle))
         {
            log.info("Unregistered handle that was not registered! " + handle + " for managedConnection: " + mc);
         }
         if (trace)
            log.trace("unregisterConnection: " + handles.size() + " handles left");
      }

      public synchronized boolean isManagedConnectionFree()
      {
         return handles.isEmpty();
      }

      protected synchronized void unregisterConnections()
      {
         try
         {
            for (Iterator i = handles.iterator(); i.hasNext(); )
            {
               getCcm().unregisterConnection(BaseConnectionManager2.this, i.next());
            }
         }
         finally
         {
            handles.clear();
         }
      }

      /**
       * May be overridden to e.g. remove from tx map.
       * @param param1 <description>
       */
      public void connectionErrorOccurred(ConnectionEvent ce)
      {
         try
         {
            unregisterConnections();
         }
         catch (Exception e)
         {
            //ignore, it wasn't checked out.
         }
         if (ce.getSource() != getManagedConnection())
            log.warn("Notified of error on a different managed connection?");
         returnManagedConnection(this, true);
      }
      public void enlist() throws SystemException
      {
      }

      public void delist() throws ResourceException
      {
      }

   }

   public static class ConnectionManagerProxy
      implements ConnectionManager, Serializable
   {

      private transient BaseConnectionManager2 realCm;
      private final ObjectName cmName;

      ConnectionManagerProxy(final BaseConnectionManager2 realCm, final ObjectName cmName)
      {
         this.realCm = realCm;
         this.cmName = cmName;
      }

      // implementation of javax.resource.spi.ConnectionManager interface

      /**
       *
       * @param mcf <description>
       * @param cri <description>
       * @return <description>
       * @exception javax.resource.ResourceException <description>
       */
      public Object allocateConnection(ManagedConnectionFactory mcf, ConnectionRequestInfo cri) throws ResourceException
      {
         return getCM().allocateConnection(mcf, cri);
      }

      private BaseConnectionManager2 getCM() throws ResourceException
      {
         if (realCm == null)
         {
            try
            {
               realCm = (BaseConnectionManager2)MBeanServerLocator.locateJBoss().getAttribute(
                  cmName,
                  "Instance");

            }
            catch (Throwable t)
            {
               Throwable t2 = JMXExceptionDecoder.decode(t);
               //log.info("Problem locating real ConnectionManager: ", t2);
               throw new ResourceException("Problem locating real ConnectionManager: " + t2);
            } // end of try-catch
         } // end of if ()
         return realCm;
      }
   }

}// BaseConnectionManager2
