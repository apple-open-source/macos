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
import java.sql.Connection;
import java.sql.SQLException;
import java.sql.CallableStatement;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCIdentityColumnCreateCommand;
import org.jboss.ejb.plugins.cmp.jdbc.SQLUtil;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCUtil;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityCommandMetaData;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.deployment.DeploymentException;

/**
 * Create command for use with Oracle that uses a sequence in conjuction with
 * a RETURNING clause to generate keys in a single statement
 * 
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public class JDBCOracleCreateCommand extends JDBCIdentityColumnCreateCommand
{
   private String sequence;
   private int pkIndex;
   private int jdbcType;

   public void init(JDBCStoreManager manager) throws DeploymentException
   {
      super.init(manager);
   }

   protected void initEntityCommand(JDBCEntityCommandMetaData entityCommand) throws DeploymentException
   {
      super.initEntityCommand(entityCommand);
      sequence = entityCommand.getAttribute("sequence");
      if (sequence == null) {
         throw new DeploymentException("Sequence must be specified");
      }
   }

   protected void initInsertSQL()
   {
      pkIndex = 1 + insertFields.length;
      jdbcType = pkField.getJDBCType().getJDBCTypes()[0];

      StringBuffer sql = new StringBuffer();
      sql.append("{call INSERT INTO ").append(entity.getTableName());
      sql.append(" (");
      SQLUtil.getColumnNamesClause(pkField, sql)
         .append(", ");

      SQLUtil.getColumnNamesClause(insertFields, sql);

      sql.append(")");
      sql.append(" VALUES (");
      sql.append(sequence+".NEXTVAL, ");
      SQLUtil.getValuesClause(insertFields, sql);
      sql.append(")");
      sql.append(" RETURNING ");
      SQLUtil.getColumnNamesClause(pkField, sql)
         .append(" INTO ? }");
      insertSQL = sql.toString();
      if (debug) {
         log.debug("Insert Entity SQL: " + insertSQL);
      }
   }

   protected PreparedStatement prepareStatement(Connection c, String sql, EntityEnterpriseContext ctx) throws SQLException
   {
      return c.prepareCall(sql);
   }

   protected int executeInsert(PreparedStatement ps, EntityEnterpriseContext ctx) throws SQLException
   {
      CallableStatement cs = (CallableStatement) ps;
      cs.registerOutParameter(pkIndex, jdbcType);
      cs.execute();
      Object pk = JDBCUtil.getParameter(log, cs, pkIndex, jdbcType, pkField.getFieldType());
      pkField.setInstanceValue(ctx, pk);
      return 1;
   }
}
