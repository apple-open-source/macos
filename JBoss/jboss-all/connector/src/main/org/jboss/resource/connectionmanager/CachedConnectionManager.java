/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Map;
import java.util.Set;
import java.util.WeakHashMap;

import javax.management.ObjectName;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionRequestInfo;
import javax.transaction.Synchronization;
import javax.transaction.SystemException;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.jboss.ejb.EnterpriseContext;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.tm.TransactionLocal;
import org.jboss.tm.usertx.client.ServerVMClientUserTransaction;

/**
 * The CachedConnectionManager mbean manages associations between meta-aware objects
 * (those accessed through interceptor chains) and connection handles, and between
 *  user transactions and connection handles.  Normally there should only be one
 * such mbean.  It is called by CachedConnectionInterceptor, UserTransaction,
 * and all BaseConnectionManager2 instances.
 *
 * Created: Sat Jan  5 18:50:27 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:E.Guib@ceyoniq.com">Erwin Guib</a>
 * @version
 * @jmx:mbean name="jboss.jca:service=CachedConnectionManager"
 *            extends="org.jboss.system.ServiceMBean"
 */
public class CachedConnectionManager
        extends ServiceMBeanSupport
        implements ServerVMClientUserTransaction.UserTransactionStartedListener,
        CachedConnectionManagerMBean
{
   private boolean specCompliant;

   private boolean debug;

   private ObjectName transactionManagerServiceName;
   private TransactionManager tm;

   /**
    * ThreadLocal that holds current calling meta-programming aware
    * object, used in case someone is idiotic enough to cache a
    * connection between invocations.and want the spec required
    * behavior of it getting hooked up to an appropriate
    * ManagedConnection on each method invocation.
    */
   private final ThreadLocal currentObjects = new ThreadLocal();

   /**
    * The variable <code>objectToConnectionManagerMap</code> holds the
    * map of meta-aware object to set of connections it holds, used by
    * the idiot spec compliant behavior.
    *
    */
   private final Map objectToConnectionManagerMap = new HashMap();

   protected boolean trace;

   /**
    * Unclosed connections by transaction
    */
   private TransactionLocal unclosedConnectionsByTransaction;

   /**
    * Connection stacktraces
    */
   private Map connectionStackTraces = new WeakHashMap();

   /**
    * Default CachedConnectionManager managed constructor for mbeans.
    * Remember that this mbean should be a singleton.
    *
    * @jmx.managed-constructor
    */
   public CachedConnectionManager()
   {
      super();
      trace = log.isDebugEnabled();
   }

   /**
    * Get the SpecCompliant value.
    * @return the SpecCompliant value.
    * @jmx.managed-attribute access="read-write"
    */
   public boolean isSpecCompliant()
   {
      return specCompliant;
   }

   /**
    * Set the SpecCompliant value.
    * @param specCompliant The new SpecCompliant value.
    * @jmx.managed-attribute
    */
   public void setSpecCompliant(boolean specCompliant)
   {
      this.specCompliant = specCompliant;
   }


   /**
    * Get the debug value.
    * @return the debug value.
    * @jmx.managed-attribute access="read-write"
    */
   public boolean isDebug()
   {
      return debug;
   }

   /**
    * Set the Debug value.
    * @param value The new debug value.
    * @jmx.managed-attribute
    */
   public void setDebug(boolean value)
   {
      this.debug = value;
   }

   /**
    * Get the TransactionManagerServiceName value.
    * @return the TransactionManagerServiceName value.
    *
    * @jmx:managed-attribute
    */
   public ObjectName getTransactionManagerServiceName()
   {
      return transactionManagerServiceName;
   }

   /**
    * Set the TransactionManagerServiceName value.
    * @param transactionManagerServiceName The new TransactionManagerServiceName value.
    *
    * @jmx:managed-attribute
    */
   public void setTransactionManagerServiceName(ObjectName transactionManagerServiceName)
   {
      this.transactionManagerServiceName = transactionManagerServiceName;
   }

   /**
    * The Instance attribute simply holds the current instance,
    * which is normally the only instance of CachedConnectionManager.
    *
    * @return a <code>CachedConnectionManager</code> value
    * @jmx.managed-attribute access="read-only"
    */
   public CachedConnectionManager getInstance()
   {
      return this;
   }

   protected void startService()
           throws Exception
   {
      tm = (TransactionManager) getServer().getAttribute(transactionManagerServiceName,
              "TransactionManager");
      unclosedConnectionsByTransaction = new TransactionLocal(tm);
      ServerVMClientUserTransaction.getSingleton().registerTxStartedListener(this);
      EnterpriseContext.setUserTransactionStartedListener(this);
   }

   protected void stopService() throws Exception
   {
      ServerVMClientUserTransaction.getSingleton().unregisterTxStartedListener(this);
      EnterpriseContext.setUserTransactionStartedListener(null);
   }

   //Object registration for meta-aware objects (i.e. this is called by interceptors)

   /**
    * Describe <code>pushMetaAwareObject</code> method here.
    * PUBLIC for TESTING PURPOSES ONLY!
    * @param key an <code>Object</code> value
    * @exception ResourceException if an error occurs
    */
   public void pushMetaAwareObject(final Object rawKey, Set unsharableResources)
           throws ResourceException
   {
      LinkedList stack = (LinkedList) currentObjects.get();
      if (stack == null)
      {
         if (trace)
            log.trace("new stack for key: " + rawKey);
         stack = new LinkedList();
         currentObjects.set(stack);
      } // end of if ()
      else
      {
         if (trace)
            log.trace("old stack for key: " + rawKey);
         //At one time I attempted to recycle connections held over method calls.
         //This caused problems if the other method call started a new transaction.
         //To assure optimal use of connections, close them before calling out.
      } // end of else
      //check for reentrancy, reconnect if not reentrant.
      //wrap key to be based on == rather than equals
      KeyConnectionAssociation key = new KeyConnectionAssociation(rawKey);
      if (specCompliant && !stack.contains(key))
      {
         reconnect(key, unsharableResources);
      }
      stack.addLast(key);
   }

   /**
    * Describe <code>popMetaAwareObject</code> method here.
    * PUBLIC for TESTING PURPOSES ONLY!
    *
    * @exception ResourceException if an error occurs
    */
   public void popMetaAwareObject(Set unsharableResources)
           throws ResourceException
   {
      LinkedList stack = (LinkedList) currentObjects.get();
      KeyConnectionAssociation oldKey = (KeyConnectionAssociation) stack.removeLast();
      if (trace)
         log.trace("popped object: " + oldKey);
      if (specCompliant)
      {
         if (!stack.contains(oldKey))
         {
            disconnect(oldKey, unsharableResources);
         } // end of if ()
      }
      else if (debug)
      {
         closeAll(oldKey.getCMToConnectionsMap());
      } // end of else

      //At one time I attempted to recycle connections held over method calls.
      //This caused problems if the other method call started a new transaction.
      //To assure optimal use of connections, close them before calling out.
   }

   KeyConnectionAssociation peekMetaAwareObject()
   {
      LinkedList stack = (LinkedList) currentObjects.get();
      if (stack == null)
      {
         return null;
      } // end of if ()
      if (!stack.isEmpty())
      {
         return (KeyConnectionAssociation) stack.getLast();
      } // end of if ()
      else
      {
         return null;
      } // end of else
   }

   //ConnectionRegistration -- called by ConnectionCacheListeners (normally ConnectionManagers)

   void registerConnection(ConnectionCacheListener cm, ConnectionListener cl, Object connection, ConnectionRequestInfo cri)
   {
      if (debug)
         connectionStackTraces.put(connection, new Exception("STACKTRACE"));

      KeyConnectionAssociation key = peekMetaAwareObject();
      if (trace)
         log.trace("registering connection from " + cm + ", connection : " + connection + ", key: " + key);
      if (key == null)
         return; //not participating properly in this management scheme.

      ConnectionRecord cr = new ConnectionRecord(cl, connection, cri);
      Map cmToConnectionsMap = key.getCMToConnectionsMap();
      Collection conns = (Collection) cmToConnectionsMap.get(cm);
      if (conns == null)
      {
         conns = new ArrayList();
         cmToConnectionsMap.put(cm, conns);
      } // end of if ()
      conns.add(cr);
   }

   void unregisterConnection(ConnectionCacheListener cm, Object c)
   {
      if (debug)
      {
         CloseConnectionSynchronization cas = getCloseConnectionSynchronization(false);
         if (cas != null)
            cas.remove(c);
      }

      KeyConnectionAssociation key = peekMetaAwareObject();
      if (trace)
         log.trace("unregistering connection from " + cm + ", object: " + c + ", key: " + key);
      if (key == null)
         return; //not participating properly in this management scheme.

      Map cmToConnectionsMap = key.getCMToConnectionsMap();
      Collection conns = (Collection) cmToConnectionsMap.get(cm);
      if (conns == null)
         return; // Can happen if connections are "passed" between contexts
      //throw new IllegalStateException("Trying to return an unknown connection1! " + c);
      //return;//???shouldn't happen.
      for (Iterator i = conns.iterator(); i.hasNext();)
      {
         if (((ConnectionRecord) i.next()).connection == c)
         {
            i.remove();
            return;
         }
      }
      throw new IllegalStateException("Trying to return an unknown connection2! " + c);
   }

   //called by UserTransaction after starting a transaction
   public void userTransactionStarted()
           throws SystemException
   {
      KeyConnectionAssociation key = peekMetaAwareObject();
      if (trace)
         log.trace("user tx started, key: " + key);
      if (key == null)
         return; //not participating properly in this management scheme.

      Map cmToConnectionsMap = key.getCMToConnectionsMap();
      for (Iterator i = cmToConnectionsMap.keySet().iterator(); i.hasNext();)
      {
         ConnectionCacheListener cm = (ConnectionCacheListener) i.next();
         Collection conns = (Collection) cmToConnectionsMap.get(cm);
         cm.transactionStarted(conns);
      } // end of for ()
   }

   /**
    * The <code>reconnect</code> method gets the cmToConnectionsMap
    * from objectToConnectionManagerMap, copies it to the key, and
    * reconnects all the connections in it.
    *
    * @param key a <code>KeyConnectionAssociation</code> value
    * @param unsharableResources a <code>Set</code> value
    * @exception ResourceException if an error occurs
    */
   private void reconnect(KeyConnectionAssociation key, Set unsharableResources)
           throws ResourceException
           //TODOappropriate cleanup???
   {
      Map cmToConnectionsMap = null;
      synchronized (objectToConnectionManagerMap)
      {
         cmToConnectionsMap = (Map) objectToConnectionManagerMap.get(key);
         if (cmToConnectionsMap == null)
         {
            return;
         } // end of if ()
      }
      key.setCMToConnectionsMap(cmToConnectionsMap);
      for (Iterator i = cmToConnectionsMap.keySet().iterator(); i.hasNext();)
      {
         ConnectionCacheListener cm = (ConnectionCacheListener) i.next();
         Collection conns = (Collection) cmToConnectionsMap.get(cm);
         cm.reconnect(conns, unsharableResources);
      } // end of for ()

   }

   private void disconnect(KeyConnectionAssociation key, Set unsharableResources)
           throws ResourceException
           //TODOappropriate cleanup???
   {
      Map cmToConnectionsMap = key.getCMToConnectionsMap();
      if (!cmToConnectionsMap.isEmpty())
      {

         synchronized (objectToConnectionManagerMap)
         {
            objectToConnectionManagerMap.put(key, cmToConnectionsMap);
         }
         for (Iterator i = cmToConnectionsMap.keySet().iterator(); i.hasNext();)
         {
            ConnectionCacheListener cm = (ConnectionCacheListener) i.next();
            Collection conns = (Collection) cmToConnectionsMap.get(cm);
            cm.disconnect(conns, unsharableResources);
         } // end of for ()
      } // end of if ()

   }

   private void closeAll(Map cmToConnectionsMap)
   {
      if (debug == false)
         return;

      for (Iterator i = cmToConnectionsMap.values().iterator(); i.hasNext();)
      {
         Collection conns = (Collection) i.next();
         for (Iterator j = conns.iterator(); j.hasNext();)
         {
            Object c = ((ConnectionRecord) j.next()).connection;
            CloseConnectionSynchronization cas = getCloseConnectionSynchronization(true);
            if (cas == null)
               closeConnection(c);
            else
               cas.add(c);
         }
      }
   }

   //shutdown method for ConnectionManager

   /**
    * Describe <code>unregisterConnectionCacheListener</code> method here.
    * This is a shutdown method called by a connection manager.  It will remove all reference
    * to that connection manager from the cache, so cached connections from that manager
    * will never be recoverable.
    * Possibly this method should not exist.
    * @param cm a <code>ConnectionCacheListener</code> value
    */
   void unregisterConnectionCacheListener(ConnectionCacheListener cm)
   {
      if (trace)
         log.trace("unregisterConnectionCacheListener: " + cm);
      synchronized (objectToConnectionManagerMap)
      {
         for (Iterator i = objectToConnectionManagerMap.values().iterator(); i.hasNext();)
         {
            Map cmToConnectionsMap = (Map) i.next();
            if (cmToConnectionsMap != null)
            {
               cmToConnectionsMap.remove(cm);
            } // end of if ()

         } // end of for ()

      }
   }

   /**
    * The class <code>KeyConnectionAssociation</code> wraps objects so they may be used in hashmaps
    * based on their object identity rather than equals implementation. Used for keys.
    *
    */
   private final static class KeyConnectionAssociation
   {
      //key
      private final Object o;

      //map of cm to list of connections for that cm.
      private Map cmToConnectionsMap;

      KeyConnectionAssociation(final Object o)
      {
         this.o = o;
      }

      public boolean equals(Object other)
      {
         return (other instanceof KeyConnectionAssociation) && o == ((KeyConnectionAssociation) other).o;
      }

      public int hashCode()
      {
         return System.identityHashCode(o);
      }

      public void setCMToConnectionsMap(Map cmToConnectionsMap)
      {
         this.cmToConnectionsMap = cmToConnectionsMap;
      }

      public Map getCMToConnectionsMap()
      {
         if (cmToConnectionsMap == null)
         {
            cmToConnectionsMap = new HashMap();
         } // end of if ()
         return cmToConnectionsMap;
      }

   }

   private void closeConnection(Object c)
   {
      try
      {
         Exception e = (Exception) connectionStackTraces.remove(c);
         Method m = c.getClass().getMethod("close", new Class[]{});
         try
         {
            if (e != null)
               log.info("Closing a connection for you.  Please close them yourself: " + c, e);
            else
               log.info("Closing a connection for you.  Please close them yourself: " + c);
            m.invoke(c, new Object[]{});
         }
         catch (Throwable t)
         {
            log.info("Throwable trying to close a connection for you, please close it yourself", t);
         }
      }
      catch (NoSuchMethodException nsme)
      {
         log.info("Could not find a close method on alleged connection objects.  Please close your own connections.");
      } // end of try-catch
   }

   private CloseConnectionSynchronization getCloseConnectionSynchronization(boolean createIfNotFound)
   {
      try
      {
         Transaction tx = tm.getTransaction();
         if (tx != null)
         {
            CloseConnectionSynchronization cas = (CloseConnectionSynchronization) unclosedConnectionsByTransaction.get(tx);
            if (cas == null && createIfNotFound)
            {
               cas = new CloseConnectionSynchronization();
               tx.registerSynchronization(cas);
               unclosedConnectionsByTransaction.set(tx, cas);
            }
            return cas;
         }
      }
      catch (Throwable t)
      {
         log.debug("Unable to determine transaction", t);
      }
      return null;
   }

   private class CloseConnectionSynchronization
           implements Synchronization
   {
      HashSet connections = new HashSet();
      boolean closing = false;

      public CloseConnectionSynchronization()
      {
      }

      public synchronized void add(Object c)
      {
         if (closing)
            return;
         connections.add(c);
      }

      public synchronized void remove(Object c)
      {
         if (closing)
            return;
         connections.remove(c);
      }

      public void beforeCompletion()
      {
         synchronized (this)
         {
            closing = true;
         }
         for (Iterator i = connections.iterator(); i.hasNext();)
            closeConnection(i.next());
         connections.clear(); // Help the GC
      }

      public void afterCompletion(int status)
      {
      }
   }

}// CachedConnectionManager
