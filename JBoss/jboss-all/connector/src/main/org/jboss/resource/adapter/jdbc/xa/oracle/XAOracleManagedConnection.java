/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.resource.adapter.jdbc.xa.oracle;

import java.sql.SQLException;
import java.util.Properties;

import javax.sql.XAConnection;
import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;

import org.jboss.resource.adapter.jdbc.xa.XAManagedConnection;
import org.jboss.resource.adapter.jdbc.xa.XAManagedConnectionFactory;

/**
 * Specialization of <code>XAManagedConnection</code> that provides
 * a workaround for oracle specific problems.
 *
 * <p>Theoretically, this wrapper could be used to convert Xid instances
 * into oracle-specific format (i.e. add necessary padding to global and
 * branch qualifiers) thus eliminating a need in XidFactory#setPad(boolean).
 * However, we need to measure performance impact first.</p>
 *
 * @version $Revision:$
 * @author  <a href="mailto:igorfie at yahoo dot com">Igor Fedorenko</a>.
 */
public class XAOracleManagedConnection extends XAManagedConnection
{

   /**
    * Constructor for XAResourceWrapper.
    * @param mcf
    * @param xaConnection
    * @param props
    * @param transactionIsolation
    * @throws SQLException
    */
   public XAOracleManagedConnection( final XAManagedConnectionFactory mcf,
                                     final XAConnection xaConnection,
                                     final Properties props,
                                     final int transactionIsolation,
                                     final int psCacheSize,
                                     final boolean doQueryTimeout)
      throws SQLException
   {
      super(mcf, xaConnection, props, transactionIsolation, psCacheSize, doQueryTimeout);
   }

   /**
    * <p>This is a workaround for a problem with oracle xa resource manager
    * which requires separate transaction branch for all connections envolved
    * into the transaction (as if these connections belong to different resource
    * managers).</p>
    *
    * @see javax.transaction.xa.XAResource#isSameRM(XAResource)
    */
   public boolean isSameRM(XAResource ignored) throws XAException
   {
      return false;
   }

}
