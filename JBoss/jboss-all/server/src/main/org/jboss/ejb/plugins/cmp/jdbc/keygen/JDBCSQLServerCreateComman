/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.ejb.plugins.cmp.jdbc.keygen;

import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.sql.ResultSet;

import javax.ejb.EJBException;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCIdentityColumnCreateCommand;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCUtil;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityCommandMetaData;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.deployment.DeploymentException;

/**
 * Create command for Microsoft SQL Server that uses the value from an IDENTITY
 * columns. By default uses "SELECT SCOPE_IDENTITY()" to reduce the impact of
 * triggers; can be overridden with "pk-sql" attribute e.g. for V7.
 * 
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public class JDBCSQLServerCreateCommand extends JDBCIdentityColumnCreateCommand
{
   protected void initEntityCommand(JDBCEntityCommandMetaData entityCommand) throws DeploymentException
   {
      super.initEntityCommand(entityCommand);
      pkSQL = entityCommand.getAttribute("pk-sql");
      if (pkSQL == null) {
         pkSQL = "SELECT SCOPE_IDENTITY()";
      }
   }

   protected void initInsertSQL()
   {
      super.initInsertSQL();
      insertSQL = insertSQL + "; " + pkSQL;
   }

   protected int executeInsert(PreparedStatement ps, EntityEnterpriseContext ctx) throws SQLException
   {
      ps.execute();
      ResultSet rs = null;
      try {
         int rows = ps.getUpdateCount();
         if (rows != 1) {
            throw new EJBException("Expected updateCount of 1, got "+rows);
         }
         if (ps.getMoreResults() == false) {
            throw new EJBException("Expected ResultSet but got an updateCount. Is NOCOUNT set for all triggers?");
         }

         rs = ps.getResultSet();
         if (!rs.next()) {
            throw new EJBException("ResultSet was empty");
         }
         pkField.loadInstanceResults(rs, 1, ctx);
         return rows;
      } catch (RuntimeException e) {
         throw e;
      } catch (Exception e) {
         // throw EJBException to force a rollback as the row has been inserted
         throw new EJBException("Error extracting generated keys", e);
      } finally {
         JDBCUtil.safeClose(rs);
      }
   }
}
