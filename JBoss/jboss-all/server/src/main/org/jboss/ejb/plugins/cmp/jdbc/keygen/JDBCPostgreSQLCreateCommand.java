/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.ejb.plugins.cmp.jdbc.keygen;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import javax.ejb.EJBException;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCIdentityColumnCreateCommand;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCUtil;
import org.jboss.ejb.plugins.cmp.jdbc.SQLUtil;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityCommandMetaData;

/**
 * Create command for PostgreSQL that fetches the currval of the sequence
 * associated with a SERIAL column in this table.
 * 
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public class JDBCPostgreSQLCreateCommand extends JDBCIdentityColumnCreateCommand
{
   private String sequence;
   private String sequenceSQL;

   public void init(JDBCStoreManager manager) throws DeploymentException
   {
      super.init(manager);
   }

   protected void initEntityCommand(JDBCEntityCommandMetaData entityCommand) throws DeploymentException
   {
      super.initEntityCommand(entityCommand);
      sequence = entityCommand.getAttribute("sequence");
      if (sequence == null) {
         sequence = entity.getTableName()
            + '_' + SQLUtil.getColumnNamesClause(pkField, new StringBuffer(20))
            + "_seq";
      }
      sequenceSQL = "SELECT currval('"+sequence+"')";
      if (debug) {
         log.debug("SEQUENCE SQL is :"+sequenceSQL);
      }
   }

   protected int executeInsert(PreparedStatement ps, EntityEnterpriseContext ctx) throws SQLException
   {
      int rows = ps.executeUpdate();

      Statement s = null;
      ResultSet rs = null;
      try {
         if (trace) {
            log.trace("Executing SQL :"+sequenceSQL);
         }
         Connection c = ps.getConnection();
         s = c.createStatement();
         rs = s.executeQuery(sequenceSQL);
         if (!rs.next()) {
            throw new EJBException("sequence sql returned an empty ResultSet");
         }
         pkField.loadInstanceResults(rs, 1, ctx);
      } catch (RuntimeException e) {
         throw e;
      } catch (Exception e) {
         // throw EJBException to force a rollback as the row has been inserted
         throw new EJBException("Error extracting generated keys", e);
      } finally {
         JDBCUtil.safeClose(rs);
         JDBCUtil.safeClose(s);
      }

      return rows;
   }
}
