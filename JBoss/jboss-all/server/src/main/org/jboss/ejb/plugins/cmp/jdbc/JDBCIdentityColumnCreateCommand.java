/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.InvocationTargetException;
import java.sql.SQLException;
import java.sql.PreparedStatement;
import java.sql.Connection;
import java.sql.Statement;
import java.sql.ResultSet;

import javax.ejb.EJBException;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCFieldBridge;
import org.jboss.ejb.EntityEnterpriseContext;

/**
 * Base class for create commands where the PK value is generated as side
 * effect of performing the insert operation. This is typically associated
 * with database platforms that use identity columns
 * 
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public abstract class JDBCIdentityColumnCreateCommand extends JDBCAbstractCreateCommand
{
   protected JDBCCMPFieldBridge pkField;
   protected String pkSQL;

   protected boolean isInsertField(JDBCFieldBridge field)
   {
      // do not include PK fields in the insert
      return super.isInsertField(field) && !field.isPrimaryKeyMember();
   }

   protected void initGeneratedFields() throws DeploymentException
   {
      super.initGeneratedFields();
      pkField = getGeneratedPKField();
   }

   protected int executeInsert(PreparedStatement ps, EntityEnterpriseContext ctx) throws SQLException
   {
      int rows = ps.executeUpdate();
      Connection c;
      Statement s = null;
      ResultSet rs = null;
      try {
         c = ps.getConnection();
         s = c.createStatement();
         rs = s.executeQuery(pkSQL);
         if (!rs.next()) {
            throw new EJBException("ResultSet was empty");
         }
         pkField.loadInstanceResults(rs, 1, ctx);
      } catch (RuntimeException e) {
         throw e;
      } catch (Exception e) {
         // throw EJBException to force a rollback as the row has been inserted
         throw new EJBException("Error extracting generated key", e);
      } finally {
         JDBCUtil.safeClose(rs);
         JDBCUtil.safeClose(s);
      }
      return rows;
   }

   /**
    * Helper for subclasses that use reflection to avoid driver dependencies.
    * @param t an Exception raised by a reflected call
    * @return SQLException extracted from the Throwable
    */
   protected SQLException processException(Throwable t)  {
      if (t instanceof InvocationTargetException) {
         t = ((InvocationTargetException) t).getTargetException();
      }
      if (t instanceof SQLException) {
         return (SQLException) t;
      }
      if (t instanceof RuntimeException) {
         throw (RuntimeException) t;
      }
      if (t instanceof Error) {
         throw (Error) t;
      }
      log.error(t);
      throw new IllegalStateException();
   }
}
