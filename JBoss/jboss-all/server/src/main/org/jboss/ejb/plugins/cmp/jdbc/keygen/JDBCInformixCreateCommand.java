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
import java.lang.reflect.Method;

import javax.ejb.EJBException;

import org.jboss.ejb.plugins.cmp.jdbc.JDBCIdentityColumnCreateCommand;
import org.jboss.ejb.plugins.cmp.jdbc.WrappedStatement;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityCommandMetaData;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.deployment.DeploymentException;

/**
 * Create command for Informix that uses the driver's getSerial method
 * to retrieve SERIAL values. Also supports SERIAL8 columns if method
 * attribute of entity-command is set to "getSerial8"
 *
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public class JDBCInformixCreateCommand extends JDBCIdentityColumnCreateCommand
{
   private static final String NAME = "class-name";
   private static final String DEFAULT_CLASS = "com.informix.jdbc.IfxStatement";
   private static final String METHOD = "method";
   private static final String DEFAULT_METHOD = "getSerial";

   private String className;
   private String methodName;
   private Method method;

   public void init(JDBCStoreManager manager) throws DeploymentException
   {
      super.init(manager);
      try
      {
         Class psClass = Thread.currentThread().getContextClassLoader().loadClass(className);
         method = psClass.getMethod(methodName, null);
      }
      catch(ClassNotFoundException e)
      {
         throw new DeploymentException("Could not load driver class: " + className, e);
      }
      catch(NoSuchMethodException e)
      {
         throw new DeploymentException("Driver does not have method: " + methodName + "()");
      }
   }

   protected void initEntityCommand(JDBCEntityCommandMetaData entityCommand) throws DeploymentException
   {
      super.initEntityCommand(entityCommand);
      className = entityCommand.getAttribute(NAME);
      if(className == null)
      {
         className = DEFAULT_CLASS;
      }
      methodName = entityCommand.getAttribute(METHOD);
      if(methodName == null)
      {
         methodName = DEFAULT_METHOD;
      }
   }

   protected int executeInsert(PreparedStatement ps, EntityEnterpriseContext ctx) throws SQLException
   {
      int rows = ps.executeUpdate();

      // remove any JCA wrappers
      while(ps instanceof WrappedStatement)
      {
         ps = (PreparedStatement)((WrappedStatement)ps).getUnderlyingStatement();
      }
      try
      {
         Number pk = (Number)method.invoke(ps, null);
         pkField.setInstanceValue(ctx, pk);
         return rows;
      }
      catch(RuntimeException e)
      {
         throw e;
      }
      catch(Exception e)
      {
         // throw EJBException to force a rollback as the row has been inserted
         throw new EJBException("Error extracting generated keys", e);
      }
   }
}
