
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.connectionmanager;

import javax.resource.spi.ConnectionEvent;
import javax.resource.spi.ConnectionEventListener;
import javax.resource.ResourceException;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionFactory;

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
 *
 * Created: Sat Jan 12 11:13:28 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
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

   protected ConnectionListener registerConnectionEventListener(ManagedConnection mc)
   {
      ConnectionListener cli = new NoTxConnectionEventListener(mc);
      mc.addConnectionEventListener(cli);
      return cli;
   }

   protected void managedConnectionDisconnected(ManagedConnection mc) throws ResourceException
   {
      NoTxConnectionEventListener cel = (NoTxConnectionEventListener)getConnectionEventListener(mc);
      if (cel == null)
      {
         throw new IllegalStateException("ManagedConnection with no ConnectionEventListener! " + mc);
      } // end of if ()
      //if there are no more handles, we can return to pool.
      if (cel.isManagedConnectionFree())
      {
         returnManagedConnection(mc, false);
      } // end of if ()

   }



   // implementation of javax.resource.spi.ConnectionEventListener interface

   private class NoTxConnectionEventListener
      extends BaseConnectionEventListener
   {


      private NoTxConnectionEventListener(final ManagedConnection mc)
      {
         super(mc);
      }


      /**
       *
       * @param param1 <description>
       */
      public void connectionClosed(ConnectionEvent ce)
      {
         getCcm().unregisterConnection(NoTxConnectionManager.this, ce.getConnectionHandle());
         try
         {
            unregisterAssociation((ManagedConnection)ce.getSource(), ce.getConnectionHandle());
            if (isManagedConnectionFree())
            {
               returnManagedConnection(this.getManagedConnection(), false);
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
