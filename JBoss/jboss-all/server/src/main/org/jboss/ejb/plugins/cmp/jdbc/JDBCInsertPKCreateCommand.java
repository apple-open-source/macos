/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.ejb.plugins.cmp.jdbc;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.SQLException;
import java.sql.ResultSet;
import javax.ejb.CreateException;
import javax.ejb.DuplicateKeyException;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCAbstractCreateCommand;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCUtil;
import org.jboss.ejb.plugins.cmp.jdbc.SQLUtil;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.deployment.DeploymentException;

/**
 * Base class for create commands that actually insert the primary key value.
 * If an exception processor is not supplied, this command will perform an
 * additional query to determine if a DuplicateKeyException should be thrown.
 *
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public abstract class JDBCInsertPKCreateCommand extends JDBCAbstractCreateCommand
{
   protected String existsSQL;

   public void init(JDBCStoreManager manager) throws DeploymentException
   {
      super.init(manager);

      // if no exception processor is defined, we will perform a existance
      // check before trying the insert to report duplicate key
      if(exceptionProcessor == null)
      {
         initExistsSQL();
      }
   }

   protected void initExistsSQL()
   {
      StringBuffer sql = new StringBuffer(300);
      sql.append(SQLUtil.SELECT).append("COUNT(*)").append(SQLUtil.FROM)
         .append(entity.getTableName())
         .append(SQLUtil.WHERE);
      SQLUtil.getWhereClause(entity.getPrimaryKeyFields(), sql);
      existsSQL = sql.toString();
      if(debug)
      {
         log.debug("Entity Exists SQL: " + existsSQL);
      }
   }

   protected void beforeInsert(EntityEnterpriseContext ctx) throws CreateException
   {
      // are we checking existance by query?
      if(existsSQL != null)
      {
         Connection c = null;
         PreparedStatement ps = null;
         ResultSet rs = null;
         try
         {
            if(debug)
               log.debug("Executing SQL: " + existsSQL);

            c = entity.getDataSource().getConnection();
            ps = c.prepareStatement(existsSQL);

            // bind PK
            // @todo add a method to EntityBridge that binds pk fields directly
            Object pk = entity.extractPrimaryKeyFromInstance(ctx);
            entity.setPrimaryKeyParameters(ps, 1, pk);

            rs = ps.executeQuery();
            if(!rs.next())
            {
               throw new CreateException("Error checking if entity with primary pk " + pk + "exists: SQL returned no rows");
            }
            if(rs.getInt(1) > 0)
            {
               throw new DuplicateKeyException("Entity with primary key " + pk + " already exists");
            }
         }
         catch(SQLException e)
         {
            log.error("Error checking if entity exists", e);
            throw new CreateException("Error checking if entity exists:" + e);
         }
         finally
         {
            JDBCUtil.safeClose(rs);
            JDBCUtil.safeClose(ps);
            JDBCUtil.safeClose(c);
         }
      }
   }
}
