/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.resource.adapter.jdbc.xa.oracle;

import java.sql.SQLException;
import java.util.Properties;

import javax.resource.spi.ManagedConnection;
import javax.sql.XAConnection;

import org.jboss.resource.adapter.jdbc.xa.XAManagedConnectionFactory;

/**
 * <code>XAManagedConnectionFactory</code> that creates instances of
 * <code>XAOracleManagedConnection</code>.
 * 
 * @version $Revision:$
 * @author  <a href="mailto:igorfie at yahoo dot com">Igor Fedorenko</a>.
 */
public class XAOracleManagedConnectionFactory extends XAManagedConnectionFactory
{
   private static final String ORACLE_XADATASOURCE = "oracle.jdbc.xa.client.OracleXADataSource";

   /**
    * Constructor for XAOracleManagedConnectionFactory.
    */
   public XAOracleManagedConnectionFactory()
   {
      super();
      
      // Provides default <code>xaDataSourceClass</code>
      setXADataSourceClass(ORACLE_XADATASOURCE);
   }

   /**
    * @see org.jboss.resource.adapter.jdbc.xa.XAManagedConnectionFactory#newXAManagedConnection(Properties, XAConnection)
    */
   protected ManagedConnection newXAManagedConnection( Properties props,
                                                       XAConnection xaConnection )
      throws SQLException
   {
      return new XAOracleManagedConnection(this, xaConnection, props, transactionIsolation, preparedStatementCacheSize, doQueryTimeout);
   }
}
