/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc.keygen;

import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import javax.ejb.CreateException;
import javax.sql.DataSource;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityCommandMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCInsertPKCreateCommand;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCUtil;

/**
 * Create command that uses an SQL statement to generate the primary key.
 * Typically used with databases that support sequences.
 *
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 *
 * @version $Revision: 1.1.2.2 $
 */
public class JDBCPkSqlCreateCommand extends JDBCInsertPKCreateCommand
{
   private String pkSQL;
   private JDBCCMPFieldBridge pkField;

   public void init(JDBCStoreManager manager) throws DeploymentException
   {
      super.init(manager);
      pkField = getGeneratedPKField();
   }

   protected void initEntityCommand(JDBCEntityCommandMetaData entityCommand) throws DeploymentException
   {
      super.initEntityCommand(entityCommand);

      pkSQL = entityCommand.getAttribute("pk-sql");
      if (pkSQL == null) {
         throw new DeploymentException("pk-sql attribute must be set for entity " + entity.getEntityName());
      }
      if (debug) {
         log.debug("Generate PK sql is: " + pkSQL);
      }
   }

   protected void generateFields(EntityEnterpriseContext ctx) throws CreateException
   {
      super.generateFields(ctx);

      Connection con = null;
      Statement s = null;
      ResultSet rs = null;
      try {
         if (debug) {
            log.debug("Executing SQL: " + pkSQL);
         }

         DataSource dataSource = entity.getDataSource();
         con = dataSource.getConnection();
         s = con.createStatement();

         rs = s.executeQuery(pkSQL);
         if (!rs.next()) {
            throw new CreateException("Error fetching next primary key value: result set contains no rows");
         }
         pkField.loadInstanceResults(rs, 1, ctx);
      } catch (SQLException e) {
         log.error("Error fetching the next primary key value", e);
         throw new CreateException("Error fetching the next primary key value:" + e);
      } finally {
         JDBCUtil.safeClose(rs);
         JDBCUtil.safeClose(s);
         JDBCUtil.safeClose(con);
      }
   }
}
