/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.dbschema.util;

import net.sourceforge.junitejb.EJBTestCase;

import javax.sql.DataSource;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import java.sql.Connection;
import java.sql.SQLException;


/**
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class AbstractDBSchemaTestCase
   extends EJBTestCase
{
   protected String datasourceName = "java:/DefaultDS";
   protected DataSource datasource;

   public AbstractDBSchemaTestCase(String s)
   {
      super(s);
   }

   protected Connection getConnection()
      throws NamingException, SQLException
   {
      return getDataSource().getConnection();
   }

   protected DataSource getDataSource() throws NamingException
   {
      if(datasource == null)
      {
         datasource = getDataSource(datasourceName);
      }
      return datasource;
   }

   protected DataSource getDataSource(String datasource) throws NamingException
   {
      InitialContext ic = null;
      try
      {
         ic = new InitialContext();
         return (DataSource)ic.lookup(datasource);
      }
      finally
      {
         if(ic != null)
            try
            {
               ic.close();
            }
            catch(Exception ignore){}
      }
   }
}
