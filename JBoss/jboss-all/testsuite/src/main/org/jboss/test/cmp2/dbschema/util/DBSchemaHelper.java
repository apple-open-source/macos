/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.dbschema.util;


import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.DatabaseMetaData;
import java.sql.Connection;
import java.sql.DriverManager;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;


/**
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class DBSchemaHelper
{
   private static final String TABLE_NAME = "TABLE_NAME";

   public static Table getTable(DatabaseMetaData dbMD, String tableName) throws SQLException
   {
      ResultSet rs = dbMD.getColumns(null, null, tableName, null);
      Map columns = new HashMap();
      while(rs.next())
      {
         final Column column = new Column(rs);
         columns.put(column.getName(), column);
      }
      safeClose(rs);
      return new Table(tableName, columns);
   }

   public static List getTableNames(DatabaseMetaData dbMD) throws SQLException
   {
      ResultSet rs = dbMD.getTables(null, null, null, null);
      List results = new ArrayList();
      while(rs.next())
      {
         results.add(rs.getString(TABLE_NAME));
      }
      safeClose(rs);
      return results;
   }

   public static Connection getConnection(String url, String user, String pwd) throws SQLException
   {
      return DriverManager.getConnection(url, user, pwd);
   }

   public static void safeClose(Connection con)
   {
      if(con != null)
         try
         {
            con.close();
         }
         catch(Exception e){}
   }

   public static void safeClose(ResultSet rs)
   {
      if(rs != null)
         try
         {
            rs.close();
         }
         catch(Exception e){}
   }
}
