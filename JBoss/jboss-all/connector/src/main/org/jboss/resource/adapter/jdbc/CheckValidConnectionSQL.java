package org.jboss.resource.adapter.jdbc;

import java.sql.Connection;
import java.sql.Statement;
import java.sql.SQLException;

/**
 * Implements check valid connection sql
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class CheckValidConnectionSQL
   implements ValidConnectionChecker
{
   String sql;

   public CheckValidConnectionSQL(String sql)
   {
      this.sql = sql;
   }

   public SQLException isValidConnection(Connection c)
   {
      try    
      {
         Statement s = c.createStatement();
         try
         {
            s.execute(sql);
            return null;
         }
         finally
         {
            s.close();
         }
      }
      catch (SQLException e)
      {
         return e;
      }
   }
}
