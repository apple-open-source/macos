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
import java.lang.reflect.Method;

import javax.ejb.EJBException;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCIdentityColumnCreateCommand;
import org.jboss.ejb.plugins.cmp.jdbc.WrappedStatement;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCUtil;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityCommandMetaData;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.deployment.DeploymentException;

/**
 * Create command for MySQL that uses the driver's getGeneratedKeys method
 * to retrieve AUTO_INCREMENT values.
 * 
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public class JDBCMySQLCreateCommand extends JDBCIdentityColumnCreateCommand
{
   private String className;
   private String methodName;
   private Method method;

   public void init(JDBCStoreManager manager) throws DeploymentException
   {
      super.init(manager);
      try {
         Class psClass = Thread.currentThread().getContextClassLoader().loadClass(className);
         method = psClass.getMethod(methodName, null);
      } catch (ClassNotFoundException e) {
         throw new DeploymentException("Could not load driver class: "+className, e);
      } catch (NoSuchMethodException e) {
         throw new DeploymentException("Driver does not have method: "+methodName+"()");
      }
   }

   protected void initEntityCommand(JDBCEntityCommandMetaData entityCommand) throws DeploymentException
   {
      super.initEntityCommand(entityCommand);
      className = entityCommand.getAttribute("class-name");
      if (className == null) {
         className = "com.mysql.jdbc.PreparedStatement";
      }
      methodName = entityCommand.getAttribute("method");
      if (methodName == null) {
         methodName = "getGeneratedKeys";
      }
   }

   protected int executeInsert(PreparedStatement ps, EntityEnterpriseContext ctx) throws SQLException
   {
      int rows = ps.executeUpdate();

      // remove any JCA wrappers
      while (ps instanceof WrappedStatement) {
         ps = (PreparedStatement) ((WrappedStatement)ps).getUnderlyingStatement();
      }

      ResultSet rs = null;
      try {
         rs = (ResultSet) method.invoke(ps, null);
         if (!rs.next()) {
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
