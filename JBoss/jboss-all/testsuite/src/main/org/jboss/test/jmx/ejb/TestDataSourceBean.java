
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.ejb;

import java.util.Collection;
import javax.ejb.*;
import javax.sql.*;
import java.sql.*;
import javax.naming.*;


/**
 *   This is a session bean whose only purpose is to look for and test datasources. It is an example of how to use the EJBDoclet tags.
 *   
 *   @ejb:stateless-session
 *   @ejb:ejb-name test/jmx/TestDataSource
 *   @ejb:jndi-name ejb/test/jmx/TestDataSource
 *   @ejb:security-role-ref admin Administrator
 *   @ejb:permission Teller
 *   @ejb:permission Administrator
 *   @ejb:transaction Required
 *   @ejb:transaction-type Container
 *
 *   JBoss specific
 *   @jboss:container-configuration Standard Stateless SessionBean
 *
 */
public class TestDataSourceBean
   implements SessionBean
{
   // Public --------------------------------------------------------
   /**
    * The <code>testDataSource</code> method looks for the datasource at the supplied name
    * and tests if it can supply a working connection.
    *
    * @param dsName a <code>String</code> value
    * @ejb:interface-method type="remote"
    */
   public void testDataSource(String dsName)
   {
      try
      {
         InitialContext ctx = new InitialContext();
         DataSource ds = (DataSource)ctx.lookup(dsName);
         if (ds == null) {
            throw new Exception("DataSource lookup was null");
         }
         Connection c = ds.getConnection();
         if (c == null) {
             throw new Exception("Connection was null!!");
         }
         DatabaseMetaData dmd = c.getMetaData();
         ResultSet rs = dmd.getTables(null, null, "%", null);
         c.close();
      } catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   /**
    * The <code>isBound</code> method checks to see if the supplied name is bound in jndi.
    *
    * @param name a <code>String</code> value
    * @return a <code>boolean</code> value
    * @ejb:interface-method type="remote"
    */
   public boolean isBound(String name)
   {
      try
      {
         InitialContext ctx = new InitialContext();
         Object ds = ctx.lookup(name);
         if (ds == null) {
            return false;
         }
         return true;
      }
      catch (NamingException e)
      {
         return false;
      } // end of catch
      
   }
   
   /**
    * Create.
    */
   public void ejbCreate()
      throws CreateException
   { 
   }
   
   // SessionBean implementation ------------------------------------
   public void ejbActivate() {}
   public void ejbPassivate() {}
   public void setSessionContext(SessionContext ctx) {}
   
   /**
    * Remove
    *
    */
   public void ejbRemove() {}

}
