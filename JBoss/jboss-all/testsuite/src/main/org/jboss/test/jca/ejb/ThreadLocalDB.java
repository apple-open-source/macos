
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jca.ejb;

import java.sql.Connection;
import java.sql.Statement;
import javax.naming.InitialContext;
import javax.sql.DataSource;

/**
 * Thread Local Database
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version
 */

public class ThreadLocalDB
{
   private static ThreadLocal tl = new ThreadLocal();

   public static Connection open()
      throws Exception
   {
      Connection c = getConnection();
      if (c == null)
      {
         InitialContext ctx = new InitialContext();
         DataSource ds = (DataSource) ctx.lookup("java:/DefaultDS");
         ctx.close();
         c = ds.getConnection();
         tl.set(c);
      }
      return c;
   }

   public static void close()
   {
      Connection c = getConnection();
      tl.set(null);
      if (c != null)
      {
         try
         {
            c.close();
         }
         catch (Exception ignored)
         {
         }
      }
   }

   private static Connection getConnection()
   {
      return (Connection) tl.get();
   }
}

