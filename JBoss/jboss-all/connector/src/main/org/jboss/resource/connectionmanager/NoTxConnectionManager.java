/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import javax.resource.ResourceException;
import javax.resource.spi.ConnectionEvent;
import javax.resource.spi.ManagedConnection;

import org.jboss.logging.Logger;

/**
 * The NoTxConnectionManager is an simple extension class of the BaseConnectionManager2
 * for use with jca adapters with no transaction support.
 *  It includes functionality to obtain managed connections from
 * a ManagedConnectionPool mbean, find the Subject from a SubjectSecurityDomain,
 * and interact with the CachedConnectionManager for connections held over
 * transaction and method boundaries.  Important mbean references are to a
 * ManagedConnectionPool supplier (typically a JBossManagedConnectionPool), and a
 * RARDeployment representing the ManagedConnectionFactory.
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.6.2.4 $
 * @jmx:mbean name="jboss.jca:service=NoTxConnectionManager"
 *            extends="BaseConnectionManager2MBean"
 */

public class NoTxConnectionManager
   extends BaseConnectionManager2
{

   /**
    * Creates a new NoTxConnectionManager instance.
    * @jmx:managed-constructor
    */
   public NoTxConnectionManager()
   {
   }

   /**
    * Creates a new NoTxConnectionManager instance.
    * for TESTING ONLY! not a managed operation.
    * @param mcf a <code>ManagedConnectionFactory</code> value
    * @param ccm a <code>CachedConnectionManager</code> value
    * @param poolingStrategy a <code>ManagedConnectionPool</code> value
    */
   public NoTxConnectionManager(CachedConnectionManager ccm,
                                ManagedConnectionPool poolingStrategy)
   {
      super(ccm, poolingStrategy);
   }

   public ConnectionListener createConnectionListener(ManagedConnection mc, Object context)
   {
      ConnectionListener cli = new NoTxConnectionEventListener(mc, poolingStrategy, context, log);
      mc.addConnectionEventListener(cli);
      return cli;
   }

   protected void managedConnectionDisconnected(ConnectionListener cl) throws ResourceException
   {
      //if there are no more handles, we can return to pool.
      if (cl.isManagedConnectionFree())
         returnManagedConnection(cl, false);
   }



   // implementation of javax.resource.spi.ConnectionEventListener interface

   private class NoTxConnectionEventListener
      extends BaseConnectionEventListener
   {
      private NoTxConnectionEventListener(final ManagedConnection mc, final ManagedConnectionPool mcp, final Object context, Logger log)
      {
         super(mc, mcp, context, log);
      }


      /**
       *
       * @param param1 <description>
       */
      public void connectionClosed(ConnectionEvent ce)
      {
         try
         {
            getCcm().unregisterConnection(NoTxConnectionManager.this, ce.getConnectionHandle());
         }
         catch (Throwable t)
         {
            log.info("Throwable from unregisterConnection", t);
         }
         try
         {
            unregisterAssociation(this, ce.getConnectionHandle());
            if (isManagedConnectionFree())
            {
               returnManagedConnection(this, false);
            }
         }
         catch (ResourceException re)
         {
            log.error("ResourceException while closing connection handle!", re);
         } // end of try-catch

      }

      /**
       *
       * @param param1 <description>
       */
      public void localTransactionStarted(ConnectionEvent ce)
      {
         //nothing to do.
      }

      /**
       *
       * @param param1 <description>
       */
      public void localTransactionCommitted(ConnectionEvent ce)
      {
         //nothing to do.
      }

      /**
       *
       * @param param1 <description>
       */
      public void localTransactionRolledback(ConnectionEvent ce)
      {
         //nothing to do.
      }

   }

}//
