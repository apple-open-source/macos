package org.jboss.resource.adapter.jdbc.vendor;

import java.lang.reflect.Method;
import java.sql.Connection;
import java.sql.SQLException;

import org.jboss.logging.Logger;
import org.jboss.resource.adapter.jdbc.ValidConnectionChecker;
import org.jboss.util.NestedRuntimeException;

/**
 * Implements check valid connection sql
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class OracleValidConnectionChecker
   implements ValidConnectionChecker
{
   private static final Logger log = Logger.getLogger(OracleValidConnectionChecker.class);

   private Method ping;

   // The timeout (apparently the timeout is ignored?)
   private static Object[] params = new Object[] { new Integer(5000) };

   public OracleValidConnectionChecker()
   {
      try
      {
         Class oracleConnection = Thread.currentThread().getContextClassLoader().loadClass("oracle.jdbc.driver.OracleConnection");
         ping = oracleConnection.getMethod("pingDatabase", new Class[] { Integer.TYPE });
      }
      catch (Exception e)
      {
         throw new NestedRuntimeException("Unable to resolve pingDatabase method:", e);
      }
   }

   public SQLException isValidConnection(Connection c)
   {
      try
      {
         Integer status = (Integer) ping.invoke(c, params);

         // Error
         if (status.intValue() < 0)
            return new SQLException("pingDatabase failed status=" + status);
      }
      catch (Exception e)
      {
         // What do we do here? Assume it is a misconfiguration
         log.warn("Unexpected error in pingDatabase", e);
      }

      // OK
      return null;
   }
}
