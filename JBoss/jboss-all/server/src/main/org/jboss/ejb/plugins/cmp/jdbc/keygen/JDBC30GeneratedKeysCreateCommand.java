/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.ejb.plugins.cmp.jdbc.keygen;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import javax.ejb.EJBException;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCIdentityColumnCreateCommand;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCUtil;
import org.jboss.ejb.EntityEnterpriseContext;

/**
 * Create method that uses the JDBC 3.0 getGeneratedKeys method to obtain
 * the value from the identity column.
 * 
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public class JDBC30GeneratedKeysCreateCommand extends JDBCIdentityColumnCreateCommand
{
   private static final Method CONNECTION_PREPARE;
   private static final Integer GENERATE_KEYS;
   private static final Method GET_GENERATED_KEYS;
   static {
      Method prepare, getGeneratedKeys;
      Integer generateKeys;
      try {
         prepare = Connection.class.getMethod("prepareStatement", new Class[] {String.class, int.class});
         getGeneratedKeys = PreparedStatement.class.getMethod("getGeneratedKeys", null);
         Field f = PreparedStatement.class.getField("RETURN_GENERATED_KEYS");
         generateKeys = (Integer) f.get(PreparedStatement.class);
      } catch (Exception e) {
         prepare = null;
         getGeneratedKeys = null;
         generateKeys = null;
      }
      CONNECTION_PREPARE = prepare;
      GET_GENERATED_KEYS = getGeneratedKeys;
      GENERATE_KEYS = generateKeys;
   }

   public void init(JDBCStoreManager manager) throws DeploymentException
   {
      if (CONNECTION_PREPARE == null) {
         throw new DeploymentException("Create command requires JDBC 3.0 (JDK1.4+)");
      }
      super.init(manager);
   }

   protected PreparedStatement prepareStatement(Connection c, String sql, EntityEnterpriseContext ctx) throws SQLException
   {
      try {
         return (PreparedStatement) CONNECTION_PREPARE.invoke(c, new Object[] { sql, GENERATE_KEYS });
      } catch (Exception e) {
         throw processException(e);
      }
   }

   protected int executeInsert(PreparedStatement ps, EntityEnterpriseContext ctx) throws SQLException
   {
      int rows = ps.executeUpdate();
      ResultSet rs = null;
      try {
         rs = (ResultSet) GET_GENERATED_KEYS.invoke(ps, null);
         if (!rs.next()) {
            // throw EJBException to force a rollback as the row has been inserted
            throw new EJBException("getGeneratedKeys returned an empty ResultSet");
         }
         pkField.loadInstanceResults(rs, 1, ctx);
      } catch (RuntimeException e) {
         throw e;
      } catch (Exception e) {
         // throw EJBException to force a rollback as the row has been inserted
         throw new EJBException("Error extracting generated keys", e);
      } finally {
         JDBCUtil.safeClose(rs);
      }
      return rows;
   }
}
