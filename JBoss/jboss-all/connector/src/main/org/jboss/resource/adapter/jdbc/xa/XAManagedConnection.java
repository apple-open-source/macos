/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.resource.adapter.jdbc.xa;

import javax.resource.spi.LocalTransaction;
import javax.sql.XAConnection;
import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;
import org.jboss.resource.adapter.jdbc.BaseWrapperManagedConnection;
import java.sql.SQLException;
import java.util.Properties;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionEvent;
import javax.resource.spi.ConnectionEventListener;
import org.jboss.resource.JBossResourceException;



/**
 * XAManagedConnection.java
 *
 *
 * Created: Mon Aug 12 23:02:44 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.3.2.11 $
 */

public class XAManagedConnection
   extends BaseWrapperManagedConnection
   implements XAResource
{

   protected final XAConnection xaConnection;

   protected final XAResource xaResource;

   protected Xid currentXid;

   public XAManagedConnection(XAManagedConnectionFactory mcf,
                              XAConnection xaConnection,
                              Properties props,
                              int transactionIsolation,
                              int psCacheSize,
                              boolean doQueryTimeout) throws SQLException
   {
      super(mcf, xaConnection.getConnection(), props, transactionIsolation, psCacheSize, doQueryTimeout);
      this.xaConnection = xaConnection;
      xaConnection.addConnectionEventListener( new javax.sql.ConnectionEventListener ()
         {
            public void connectionClosed(javax.sql.ConnectionEvent ce)
            {
               //only we can do this, ignore
            }

            public void connectionErrorOccurred(javax.sql.ConnectionEvent ce)
            {
               SQLException ex = ce.getSQLException();
               broadcastConnectionError(ex);
            }
         }
                                               );
      this.xaResource = xaConnection.getXAResource();
   }

   /**
    * Describe <code>broadcastConnectionError</code> method here.
    * this is present so the ConnectionEventListener inner class can access the method.
    * @param e a <code>SQLException</code> value
    */
   protected void broadcastConnectionError(SQLException e)
   {
      super.broadcastConnectionError(e);
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public LocalTransaction getLocalTransaction() throws ResourceException
   {
      throw new JBossResourceException("xa tx only!");
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public XAResource getXAResource() throws ResourceException
   {
      return this;
   }

   /**
    *
    * @exception javax.resource.ResourceException <description>
    */
   public void destroy() throws ResourceException
   {
      try
      {
         super.destroy();
      }
      finally
      {
         try
         {
            xaConnection.close();
         }
         catch (SQLException e)
         {
            checkException(e);
         } // end of try-catch
      } // end of try-catch
   }

   // implementation of javax.transaction.xa.XAResource interface

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void start(Xid xid, int flags) throws XAException
   {
      try
      {
         checkState();
      }
      catch (SQLException e)
      {
         getLog().warn("Error setting state ", e);
      }
      xaResource.start(xid, flags);
      currentXid = xid;
      inManagedTransaction = true;
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void end(Xid xid, int flags) throws XAException
   {
      xaResource.end(xid, flags);
      //we want to allow ending transactions that are not the current
      //one.  When one does this, inManagedTransaction is still true.
      if (currentXid != null && currentXid.equals(xid))
      {
         inManagedTransaction = false;
         currentXid = null;
      }
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public int prepare(Xid xid) throws XAException
   {
      return xaResource.prepare(xid);
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void commit(Xid xid, boolean onePhase) throws XAException
   {
      xaResource.commit(xid, onePhase);
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void rollback(Xid xid) throws XAException
   {
      xaResource.rollback(xid);
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void forget(Xid xid) throws XAException
   {
      xaResource.forget(xid);
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public Xid[] recover(int flag) throws XAException
   {
      return xaResource.recover(flag);
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public boolean isSameRM(XAResource other) throws XAException
   {
      Boolean overrideValue = ((XAManagedConnectionFactory)mcf).getIsSameRMOverrideValue();
      if (overrideValue != null)
      {
         return overrideValue.booleanValue();
      } // end of if ()

      // compare apples to apples
      return (other instanceof XAManagedConnection)?
            xaResource.isSameRM(((XAManagedConnection)other).xaResource):
            xaResource.isSameRM(other);
   }

   /**
    *
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public int getTransactionTimeout() throws XAException
   {
      return xaResource.getTransactionTimeout();
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public boolean setTransactionTimeout(int seconds) throws XAException
   {
      return xaResource.setTransactionTimeout(seconds);
   }


   /**
    * Describe <code>getProps</code> method here.
    * for the mcf to access in matchManagedConnection
    * @return a <code>Properties</code> value
    */
   Properties getProps()
   {
      return props;
   }



}// XAManagedConnection
