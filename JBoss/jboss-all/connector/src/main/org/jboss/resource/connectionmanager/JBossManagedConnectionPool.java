/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import javax.management.Notification;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;

import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.system.ServiceMBeanSupport;

/**
 * The JBossManagedConnectionPool mbean configures and supplies pooling of
 * JBossConnectionEventListeners to the BaseConnectionManager2 mbean.<p>
 *   
 * It may be replaced by any mbean with a readable ManagedConnectionPool attribute
 * of type ManagedConnectionPool.  Normal pooling parameters are supplied,
 * and the criteria to distinguish ManagedConnections is set in the Criteria attribute.
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.8.2.15 $
 * @jmx:mbean name="jboss.jca:service=JBossManagedConnectionPool"
 *            extends="org.jboss.system.ServiceMBean"
 */

public class JBossManagedConnectionPool
   extends ServiceMBeanSupport
   implements JBossManagedConnectionPoolMBean,
              NotificationListener
{
   /** The managed connection factory name */
   private ObjectName managedConnectionFactoryName;

   /** The pooling criteria */
   private String criteria;

   /** The pooling strategy */
   private ManagedConnectionPool poolingStrategy;

   /** The pooling parameters */
   private final InternalManagedConnectionPool.PoolParams poolParams = new InternalManagedConnectionPool.PoolParams();

   /** Whether to use separate pools for transactional and non-transaction use */
   private boolean noTxSeparatePools;

   /**
    * Default managed JBossManagedConnectionPool constructor for mbeans.
    *
    * @jmx.managed-constructor
    */
   public JBossManagedConnectionPool()
   {
   }

   /**
    * ManagedConnectionPool is a read only attribute returning the pool
    * set up by this mbean.
    *
    * @return the ManagedConnectionPool implementing the pool configured by this mbean.
    * @jmx.managed-attribute access="read-only"
    */
   public ManagedConnectionPool getManagedConnectionPool()
   {
      return poolingStrategy;
   }

   /**
    * ManagedConnectionFactoryName holds the ObjectName of the mbean that
    * represents the ManagedConnectionFactory.  Normally this can be an
    * embedded mbean in a depends element rather than a separate mbean
    * reference.
    * @return the ManagedConnectionFactoryName value.
    * @jmx:managed-attribute
    */
   public ObjectName getManagedConnectionFactoryName()
   {
      return managedConnectionFactoryName;
   }

   /**
    * Set the ManagedConnectionFactoryName value.
    * @param newManagedConnectionFactoryName The new ManagedConnectionFactoryName value.
    * @jmx:managed-attribute
    */
   public void setManagedConnectionFactoryName(ObjectName newManagedConnectionFactoryName)
   {
      this.managedConnectionFactoryName = newManagedConnectionFactoryName;
   }

   /**
    * Get number of available free connections
    *
    * @return number of available connections
    * @jmx:managed-attribute
    */
   public long getAvailableConnectionCount()
   {
      return poolingStrategy.getAvailableConnectionCount();
   }

   /**
    *
    *
    * @return max number of connections ever used
    * @jmx:managed-attribute
    */
   public long getMaxConnectionsInUseCount()
   {
      return poolingStrategy.getMaxConnectionsInUseCount();
   }

   /**
    * Get number of connections currently in use
    *
    * @return number of connections currently in use
    * @jmx:managed-attribute
    */
   public long getInUseConnectionCount ()
   {
      return poolingStrategy.getInUseConnectionCount();
   }

   /**
    * The MinSize attribute indicates the minimum number of connections this
    * pool should hold.  These are not created until a Subject is known from a
    * request for a connection.  MinSize connections will be created for
    * each sub-pool.
    *
    * @return the MinSize value.
    * @jmx:managed-attribute
    */
   public int getMinSize()
   {
      return poolParams.minSize;
   }

   /**
    * Set the MinSize value.
    * @param newMinSize The new MinSize value.
    * @jmx:managed-attribute
    */
   public void setMinSize(int newMinSize)
   {
      poolParams.minSize = newMinSize;
   }

   /**
    * The MaxSize attribute indicates the maximum number of connections for a
    * pool. No more than MaxSize connections will be created in each
    * sub-pool.
    *
    * @return the MaxSize value.
    * @jmx:managed-attribute
    */
   public int getMaxSize()
   {
      return poolParams.maxSize;
   }

   /**
    * Set the MaxSize value.
    * @param newMaxSize The new MaxSize value.
    * @jmx:managed-attribute
    */
   public void setMaxSize(int newMaxSize)
   {
      poolParams.maxSize = newMaxSize;
   }

   /**
    * The BlockingTimeoutMillis attribute indicates the maximum time to block
    * while waiting for a connection before throwing an exception.  Note that
    * this blocks only while waiting for a permit for a connection, and will
    * never throw an exception if creating a new connection takes an
    * inordinately long time.
    *
    * @return the BlockingTimeout value.
    * @jmx:managed-attribute
    */
   public int getBlockingTimeoutMillis()
   {
      return poolParams.blockingTimeout;
   }

   /**
    * Set the BlockingTimeout value.
    * @param newBlockingTimeout The new BlockingTimeout value.
    * @jmx:managed-attribute
    */
   public void setBlockingTimeoutMillis(int newBlockingTimeout)
   {
      poolParams.blockingTimeout = newBlockingTimeout;
   }

   /**
    * The IdleTimeoutMinutes attribute indicates the maximum time a connection
    * may be idle before being closed.  The actual maximum time depends also
    * on the IdleRemover scan time, which is 1/2 the smallest IdleTimeout of
    * any pool.
    *
    * @return the IdleTimeoutMinutes value.
    * @jmx:managed-attribute
    */
   public long getIdleTimeoutMinutes()
   {
      return poolParams.idleTimeout / (1000 * 60);
   }

   /**
    * Set the IdleTimeoutMinutes value.
    * @param newIdleTimeoutMinutes The new IdleTimeoutMinutes value.
    * @jmx:managed-attribute
    */
   public void setIdleTimeoutMinutes(long newIdleTimeoutMinutes)
   {
      poolParams.idleTimeout = newIdleTimeoutMinutes * 1000 * 60;
   }

   /**
    * Get the IdleTimeout value.
    *
    * @return the IdleTimeout value.
    */
   public long getIdleTimeout()
   {
      return poolParams.idleTimeout;
   }

   /**
    * Set the IdleTimeout value.
    *
    * @param newIdleTimeout The new IdleTimeout value.
    */
   public void setIdleTimeout(long newIdleTimeout)
   {
      poolParams.idleTimeout = newIdleTimeout;
   }

   /**
    * The Criteria attribute indicates if Subject (from security domain) or app supplied
    * parameters (such as from getConnection(user, pw)) are used to distinguish
    * connections in the pool. Choices are
    *   ByContainerAndApplication (use both),
    *   ByContainer (use Subject),
    *   ByApplication (use app supplied params only),
    *   ByNothing (all connections are equivalent, usually if adapter supports
    *     reauthentication)
    * @return the Criteria value.
    * @jmx:managed-attribute
    */
   public String getCriteria()
   {
      return criteria;
   }

   /**
    * Set the Criteria value.
    * @param newCriteria The new Criteria value.
    * @jmx:managed-attribute
    */
   public void setCriteria(String newCriteria)
   {
      this.criteria = newCriteria;
   }

   /**
    * Separate pools for transactional use
    *
    * @return true when connections should have different pools for transactional and non-transaction use.
    * @jmx:managed-attribute
    */
   public boolean getNoTxSeparatePools()
   {
      return noTxSeparatePools;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setNoTxSeparatePools(boolean value)
   {
      this.noTxSeparatePools = value;
   }

   /**
    * The <code>flush</code> method puts all currently checked out
    * connections on a list to be destroyed when returned and disposes
    * of all current pooled connections.
    *
    * @jmx.managed-operation
    */
   public void flush()
   {
      poolingStrategy.flush();
   }

   /**
    * Retrieve the connection count.
    *
    * @return the connection count
    * @jmx:managed-attribute
    */
   public int getConnectionCount()
   {
      return (poolingStrategy == null)? 0: poolingStrategy.getConnectionCount();
   }

   /**
    * Retrieve the connection created count.
    *
    * @return the connection created count
    * @jmx:managed-attribute
    */
   public int getConnectionCreatedCount()
   {
      return (poolingStrategy == null)? 0: poolingStrategy.getConnectionCreatedCount();
   }

   /**
    * Retrieve the destrooyed count.
    *
    * @return the destroyed count
    * @jmx:managed-attribute
    */
   public int getConnectionDestroyedCount()
   {
      return (poolingStrategy == null)? 0: poolingStrategy.getConnectionDestroyedCount();
   }

   //serviceMBeanSupport

   public String getName()
   {
      return "JBossManagedConnectionPool";
   }

   protected void startService() throws Exception
   {
      ManagedConnectionFactory mcf = null;
      if (managedConnectionFactoryName == null)
         throw new DeploymentException("ManagedConnectionFactory not set!");

      try
      {
         //We are getting the actual mcf instance itself.  This will require
         //some work if the mcf is an xmbean of itself.
         mcf = (ManagedConnectionFactory)server.getAttribute(managedConnectionFactoryName, "McfInstance");
      }
      catch (Exception e)
      {
         JMXExceptionDecoder.rethrow(e);
      } // end of try-catch
      getServer().addNotificationListener
      (
         managedConnectionFactoryName,
         this,
         new NotificationFilter()
         {
            public boolean isNotificationEnabled(Notification n)
            {
               return RARDeployment.MCF_ATTRIBUTE_CHANGED_NOTIFICATION.equals(n.getType())
                      && managedConnectionFactoryName.equals(n.getSource());
            }
         },
         null
      );

      if ("ByContainerAndApplication".equals(criteria))
         poolingStrategy = new PoolBySubjectAndCri(mcf, poolParams, noTxSeparatePools, log);
      else if ("ByContainer".equals(criteria))
         poolingStrategy = new PoolBySubject(mcf, poolParams, noTxSeparatePools, log);
      else if ("ByApplication".equals(criteria))
         poolingStrategy = new PoolByCri(mcf, poolParams, noTxSeparatePools, log);
      else if ("ByNothing".equals(criteria))
         poolingStrategy = new OnePool(mcf, poolParams, noTxSeparatePools, log);
      else
         throw new DeploymentException("Unknown pooling criteria: " + criteria);
   }

   protected void stopService() throws Exception
   {
      poolingStrategy.shutdown();
      getServer().removeNotificationListener(managedConnectionFactoryName, this);
      poolingStrategy = null;
   }

   public void handleNotification(Notification notification,
                                  Object handback)
   {
      flush();
   }

   //pooling strategies
   //ManagedConnectionPool implementations

   //base class


   /**
    * The base pool implementation
    */
   private abstract static class BasePool implements ManagedConnectionPool
   {
      /** The subpools */
      private final Map pools = new HashMap();

      /** The managed connection factory */
      private final ManagedConnectionFactory mcf;
      
      /** The connection listener factory */
      private ConnectionListenerFactory clf;

      /** The pool parameters */
      private final InternalManagedConnectionPool.PoolParams poolParams;

      /** The in use count */
      private int inuseCount;

      /** Whether to use separate pools for transactional and non-transaction use */
      private boolean noTxSeparatePools;

      /** The logger */
      private final Logger log;

      /** Is trace enabled */
      private boolean traceEnabled = false;

      /**
       * Create a new base pool
       * 
       * @param mcf the managed connection factory
       * @param poolParams the pooling parameters
       * @param log the log
       */
      public BasePool(final ManagedConnectionFactory mcf, final InternalManagedConnectionPool.PoolParams poolParams,
                      final boolean noTxSeparatePools, final Logger log)
      {
         this.mcf = mcf;
         this.poolParams = poolParams;
         this.noTxSeparatePools = noTxSeparatePools;
         this.log = log;
         this.traceEnabled = log.isTraceEnabled();
      }

      /**
       * Retrieve the key for this request
       * 
       * @param subject the subject 
       * @param cri the connection request information
       * @return the key
       * @throws ResourceException for any error
       */
      protected abstract Object getKey(Subject subject, ConnectionRequestInfo cri, boolean separateNoTx) throws ResourceException;

      public ManagedConnectionFactory getManagedConnectionFactory()
      {
         return mcf;
      }

      public void setConnectionListenerFactory(ConnectionListenerFactory clf)
      {
         this.clf = clf;
      }

      /**
       * Return the inuse count
       * 
       * @return the count
       */
      public int getInUseConnectionCount()
      {
         return inuseCount;
      }

      public ConnectionListener getConnection(Subject subject, ConnectionRequestInfo cri)
         throws ResourceException
      {
         InternalManagedConnectionPool mcp = getPool(subject, cri);
         ConnectionListener cl = mcp.getConnection(subject, cri);

         ++inuseCount;
         if (traceEnabled)
            dump("Getting connection from pool");
         return cl;
      }

      public void returnConnection(ConnectionListener cl, boolean kill) throws ResourceException
      {
         InternalManagedConnectionPool mcp = (InternalManagedConnectionPool) cl.getContext();
         --inuseCount;
         mcp.returnConnection(cl, kill);
         if (traceEnabled)
            dump("Returning connection to pool");
      }
      
      public int getConnectionCount()
      {
         int count = 0;
         synchronized (pools)
         {
            for (Iterator i = pools.values().iterator(); i.hasNext(); )
               count += ((InternalManagedConnectionPool)i.next()).getConnectionCount();
         }
         return count;
      }

      public int getConnectionCreatedCount()
      {
         int count = 0;
         synchronized (pools)
         {
            for (Iterator i = pools.values().iterator(); i.hasNext(); )
               count += ((InternalManagedConnectionPool)i.next()).getConnectionCreatedCount();
         }
         return count;
      }

      public int getConnectionDestroyedCount()
      {
         int count = 0;
         synchronized (pools)
         {
            for (Iterator i = pools.values().iterator(); i.hasNext(); )
               count += ((InternalManagedConnectionPool)i.next()).getConnectionDestroyedCount();
         }
         return count;
      }

      public long getAvailableConnectionCount()
      {
         long count = 0;
         synchronized (pools)
         {
            if (pools.size() == 0)
               return poolParams.maxSize;
            for (Iterator i = pools.values().iterator(); i.hasNext(); )
               count += ((InternalManagedConnectionPool)i.next()).getAvailableConnections();
         }
         return count;
      }

      public int getMaxConnectionsInUseCount()
      {
         int count = 0;
         synchronized (pools)
         {
            for (Iterator i = pools.values().iterator(); i.hasNext(); )
               count += ((InternalManagedConnectionPool)i.next()).getMaxConnectionsInUseCount();
         }
         return count;
      }
      public void shutdown()
      {
         synchronized (pools)
         {
            for (Iterator i = pools.values().iterator(); i.hasNext(); )
               ((InternalManagedConnectionPool)i.next()).shutdown();
            pools.clear();
         }
      }

      public void flush()
      {
         synchronized (pools)
         {
            for (Iterator i = pools.values().iterator(); i.hasNext(); )
               ((InternalManagedConnectionPool)i.next()).flush();
            pools.clear();
         }
      }

      /**
       * Determine the correct pool for this request,
       * creates a new one when necessary
       * 
       * @param subject the subject
       * @param cri the connection request information
       * @return the pool
       * @throws ResourceException for any error
       */
      private InternalManagedConnectionPool getPool(Subject subject, ConnectionRequestInfo cri)
         throws ResourceException
      {
         boolean separateNoTx = false;
         if (noTxSeparatePools)
            separateNoTx = clf.isTransactional();

         InternalManagedConnectionPool mcp = null;
         Object key = getKey(subject, cri, separateNoTx);
         synchronized (pools)
         {
            mcp = (InternalManagedConnectionPool) pools.get(key);
            if (mcp == null)
            {
               mcp = new InternalManagedConnectionPool(mcf, clf, subject, cri, poolParams, log);
               pools.put(key, mcp);
            }
         }
         return mcp;
      }

      /**
       * Dump the stats to the trace log
       * 
       * @param info some context
       */
      private void dump(String info)
      {
         if (traceEnabled)
         {
            StringBuffer toLog = new StringBuffer(100);
            toLog.append(info).append(" [InUse/Available/Max]: [");
            toLog.append(this.getInUseConnectionCount()).append("/");
            toLog.append(this.getConnectionCount()).append("/");
            toLog.append(this.poolParams.maxSize);
            toLog.append("]");;
            log.trace(toLog);
         }
      }
   }

   /**
    * Pooling by subject and connection request information
    */
   private static class PoolBySubjectAndCri
      extends BasePool
   {
      public PoolBySubjectAndCri(final ManagedConnectionFactory mcf,
                                 final InternalManagedConnectionPool.PoolParams poolParams,
                                 final boolean noTxSeparatePools, 
                                 final Logger log)
      {
         super(mcf, poolParams, noTxSeparatePools, log);
      }

      protected Object getKey(final Subject subject, final ConnectionRequestInfo cri, final boolean separateNoTx) throws ResourceException
      {
         return new SubjectCriKey(subject, cri, separateNoTx);
      }
   }

   /**
    * Pool by subject and criteria
    */
   private static class SubjectCriKey
   {
      /** Identifies no subject */
      private static final Object NOSUBJECT = new Object();
      
      /** Identifies no connection request information */
      private static final Object NOCRI = new Object();

      /** The subject */
      private final Object subject;
      
      /** The connection request information */
      private final Object cri;

      /** Separate no tx */
      private boolean separateNoTx;

      SubjectCriKey(Subject subject, ConnectionRequestInfo cri, boolean separateNoTx)
      {
         this.subject = (subject == null)? NOSUBJECT:subject;
         this.cri = (cri == null)? NOCRI:cri;
         this.separateNoTx = separateNoTx;
      }

      public int hashCode()
      {
         return subject.hashCode() ^ cri.hashCode();
      }

      public boolean equals(Object obj)
      {
         if (this == obj)
            return true;
         if (obj == null || (obj instanceof SubjectCriKey) == false)
            return false;
         SubjectCriKey other = (SubjectCriKey) obj;
         return subject.equals(other.subject)  && cri.equals(other.cri) && separateNoTx == other.separateNoTx;
      }
   }

   /**
    * Pool by subject
    */
   private static class PoolBySubject
      extends BasePool
   {

      public PoolBySubject(final ManagedConnectionFactory mcf,
                           final InternalManagedConnectionPool.PoolParams poolParams,
                           final boolean noTxSeparatePools, 
                           final Logger log)
      {
         super(mcf, poolParams, noTxSeparatePools, log);
      }

      protected Object getKey(final Subject subject, final ConnectionRequestInfo cri, boolean separateNoTx)
      {
         return new SubjectKey(subject, separateNoTx);
      }
   }

   /**
    * Pool by subject
    */
   private static class SubjectKey
   {
      /** Identifies no subject */
      private static final Object NOSUBJECT = new Object();

      /** The subject */
      private final Object subject;

      /** Separate no tx */
      private boolean separateNoTx;

      SubjectKey(Subject subject, boolean separateNoTx)
      {
         this.subject = (subject == null)? NOSUBJECT:subject;
         this.separateNoTx = separateNoTx;
      }

      public int hashCode()
      {
         return subject.hashCode();
      }

      public boolean equals(Object obj)
      {
         if (this == obj)
            return true;
         if (obj == null || (obj instanceof SubjectKey) == false)
            return false;
         SubjectKey other = (SubjectKey) obj;
         return subject.equals(other.subject)   && separateNoTx == other.separateNoTx;
      }
   }

   /**
    * Pool by connection request information
    */
   private static class PoolByCri
      extends BasePool
   {

      public PoolByCri(final ManagedConnectionFactory mcf,
                       final InternalManagedConnectionPool.PoolParams poolParams,
                       final boolean noTxSeparatePools, 
                       final Logger log)
      {
         super(mcf, poolParams, noTxSeparatePools, log);
      }

      protected Object getKey(final Subject subject, final ConnectionRequestInfo cri, boolean separateNoTx)
      {
         return new CriKey(cri, separateNoTx);
      }
   }

   /**
    * Pool by subject and criteria
    */
   private static class CriKey
   {
      /** Identifies no connection request information */
      private static final Object NOCRI = new Object();
      
      /** The connection request information */
      private final Object cri;

      /** Separate no tx */
      private boolean separateNoTx;

      CriKey(ConnectionRequestInfo cri, boolean separateNoTx) 
      {
         this.cri = (cri == null)? NOCRI:cri;
         this.separateNoTx = separateNoTx;
      }

      public int hashCode()
      {
         return cri.hashCode();
      }

      public boolean equals(Object obj)
      {
         if (this == obj)
            return true;
         if (obj == null || (obj instanceof CriKey) == false)
            return false;
         CriKey other = (CriKey) obj;
         return cri.equals(other.cri) && separateNoTx == other.separateNoTx;
      }
   }

   /**
    * One pool
    */
   public static class OnePool
      extends BasePool
   {
      public OnePool(final ManagedConnectionFactory mcf,
                     final InternalManagedConnectionPool.PoolParams poolParams,
                     final boolean noTxSeparatePools, 
                     final Logger log)
      {
         super(mcf, poolParams, noTxSeparatePools, log);
      }

      protected Object getKey(final Subject subject, final ConnectionRequestInfo cri, boolean separateNoTx)
      {
         if (separateNoTx)
            return Boolean.TRUE;
         else
            return Boolean.FALSE;
      }
   }
}
