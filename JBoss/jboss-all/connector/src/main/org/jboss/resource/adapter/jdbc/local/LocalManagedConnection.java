/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.resource.adapter.jdbc.local;

import org.jboss.resource.adapter.jdbc.BaseWrapperManagedConnection;
import java.io.PrintWriter;
import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Savepoint;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Properties;
import java.util.Set;
import org.jboss.resource.JBossResourceException;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionEvent;
import javax.resource.spi.ConnectionEventListener;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.LocalTransaction;
import javax.resource.spi.LocalTransaction;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionMetaData;
import javax.resource.spi.security.PasswordCredential;
import javax.security.auth.Subject;
import javax.transaction.xa.XAResource;


/**
 * LocalManagedConnection.java
 *
 *
 * Created: Mon Aug 12 19:18:58 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class LocalManagedConnection
   extends BaseWrapperManagedConnection
   implements LocalTransaction
{
   public LocalManagedConnection (final LocalManagedConnectionFactory mcf,
                                  final Connection con,
                                  final Properties props,
                                  final int transactionIsolation,
                                  final int psCacheSize,
                                  final boolean doQueryTimeout)
      throws SQLException
   {
      super(mcf, con, props, transactionIsolation, psCacheSize, doQueryTimeout);
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public LocalTransaction getLocalTransaction() throws ResourceException
   {
      return this;
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public XAResource getXAResource() throws ResourceException
   {
      throw new JBossResourceException("Local tx only!");
   }

   // implementation of javax.resource.spi.LocalTransaction interface

   /**
    *
    * @exception javax.resource.ResourceException <description>
    */
   public void commit() throws ResourceException
   {
      if (inManagedTransaction)
      {
         try
         {
            inManagedTransaction = false;
            con.commit();
         }
         catch (SQLException e)
         {
            checkException(e);
         } // end of try-catch
      } // end of if ()
      else
      {
         throw new JBossResourceException("Trying to commit outside of a local tx");
      } // end of else

   }

   /**
    *
    * @exception javax.resource.ResourceException <description>
    */
   public void rollback() throws ResourceException
   {
      if (inManagedTransaction)
      {
         try
         {
            inManagedTransaction = false;
            con.rollback();
         }
         catch (SQLException e)
         {
            e.printStackTrace();
            try
            {
                checkException(e);
            }
            catch (Exception e2) {}
         } // end of try-catch
      } // end of if ()
      else
      {
         throw new JBossResourceException("Trying to rollback outside of a local tx");
      } // end of else
   }

   /**
    *
    * @exception javax.resource.ResourceException <description>
    */
   public void begin() throws ResourceException
   {
      if (!inManagedTransaction)
      {
         try
         {
            if (underlyingAutoCommit)
            {
               underlyingAutoCommit = false;
               con.setAutoCommit(false);
            }
            checkState();
            inManagedTransaction = true;
         }
         catch (SQLException e)
         {
            checkException(e);
         } // end of try-catch
      } // end of if ()
      else
      {
         throw new JBossResourceException("Trying to begin a nested local tx");
      } // end of else
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


}// LocalManagedConnection
