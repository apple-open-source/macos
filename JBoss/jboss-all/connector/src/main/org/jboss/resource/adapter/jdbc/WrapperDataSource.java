/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.adapter.jdbc;


import java.io.PrintWriter;
import java.sql.Connection;
import java.sql.SQLException;
import javax.naming.NamingException;
import javax.naming.Reference;
import javax.resource.Referenceable;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionManager;
import javax.resource.spi.ConnectionRequestInfo;
import javax.sql.DataSource;
import org.jboss.util.NestedSQLException;
import java.io.Serializable;

/**
 * WrapperDataSource.java
 *
 *
 * Created: Fri Apr 19 13:34:35 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class WrapperDataSource 
   implements Referenceable, DataSource, Serializable  
{

   private final BaseWrapperManagedConnectionFactory mcf;
   private final ConnectionManager cm;

   private Reference reference;

   public WrapperDataSource (final BaseWrapperManagedConnectionFactory mcf, final ConnectionManager cm)
   {
      this.mcf = mcf;
      this.cm = cm;   
   }
   // implementation of javax.sql.DataSource interface

   /**
    *
    * @return <description>
    * @exception java.sql.SQLException <description>
    */
   public PrintWriter getLogWriter() throws SQLException
   {
      // TODO: implement this javax.sql.DataSource method
      return null;
   }

   /**
    *
    * @param param1 <description>
    * @exception java.sql.SQLException <description>
    */
   public void setLogWriter(PrintWriter param1) throws SQLException
   {
      // TODO: implement this javax.sql.DataSource method
   }

   /**
    *
    * @return <description>
    * @exception java.sql.SQLException <description>
    */
   public int getLoginTimeout() throws SQLException
   {
      // TODO: implement this javax.sql.DataSource method
      return 0;
   }

   /**
    *
    * @param param1 <description>
    * @exception java.sql.SQLException <description>
    */
   public void setLoginTimeout(int param1) throws SQLException
   {
      // TODO: implement this javax.sql.DataSource method
   }

   /**
    *
    * @return <description>
    * @exception java.sql.SQLException <description>
    */
   public Connection getConnection() throws SQLException
   {
      try 
      {
         return (Connection)cm.allocateConnection(mcf, null);
      }
      catch (ResourceException re)
      {
         throw new NestedSQLException(re);
      } // end of try-catch
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @return <description>
    * @exception java.sql.SQLException <description>
    */
   public Connection getConnection(String user, String password) throws SQLException
   {
      ConnectionRequestInfo cri = new WrappedConnectionRequestInfo(user, password);
      try 
      {
         return (Connection)cm.allocateConnection(mcf, cri);
      }
      catch (ResourceException re)
      {
         throw new NestedSQLException(re);
      } // end of try-catch
   }

   // implementation of javax.resource.Referenceable interface

   /**
    *
    * @param param1 <description>
    */
   public void setReference(final Reference reference)
   {
      this.reference = reference;
   }
   // implementation of javax.naming.Referenceable interface

   /**
    *
    * @return <description>
    * @exception javax.naming.NamingException <description>
    */
   public Reference getReference()
   {
      return reference;
   }


}// WrapperDataSource
