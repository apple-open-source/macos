/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.test.cmp2.idxandusersql.test;

import java.sql.Connection;
import java.sql.DatabaseMetaData;
import java.sql.ResultSet;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.sql.DataSource;

import junit.framework.Test;
import net.sourceforge.junitejb.EJBTestCase;
import org.apache.log4j.Logger;
import org.jboss.test.JBossTestCase;

/**
 * Test the <dbindex> and <post-table-create> features
 * @author heiko.rupp@cellent.de
 * @version $Revision: 1.1.2.5 $
 */
public class IdxAndUsersqlUnitTestCase extends EJBTestCase
{

   static final Logger log = Logger.getLogger(IdxAndUsersqlUnitTestCase.class);
   InitialContext ic = null;

   public static Test suite() throws Exception
   {
      return JBossTestCase.getDeploySetup(
         IdxAndUsersqlUnitTestCase.class,
         "cmp2-idxandusersql.jar");
   }

   public IdxAndUsersqlUnitTestCase(String name)
   {
      super(name);
      try
      {
         ic = new InitialContext();
      }
      catch (NamingException e)
      {
         ic = null;
      }
   }

   public void setUpEJB() throws Exception
   {
      super.setUpEJB();
   }

   /**
    * Check if we use the HSQL Database, as the way how the column
    * names are stored in the database (upper vs lower case) is probably
    * database specific.
    * For HSQL this is currently in uppercase, as it is for the tablenames
    * @throws Exception
    */

   public void testHsqlDriver() throws Exception
   {
      Connection cConn = getConnection();
      DatabaseMetaData dMeta = cConn.getMetaData();

      String driver = dMeta.getDriverName();
      String version = dMeta.getDriverVersion();

      cConn.close();
      assertTrue("Error: We are not using HSQL", driver.startsWith("HSQL Database"));

   }

   public void testDBIndexFoo() throws Exception
   {
      boolean found = false;

      // Column FOO should be indexed via <dbindex/>
      found = lookForIndexOnTable("DBINDEXTEST", "FOO");
      assertTrue("Error: column foo is not indexed", found);

   }

   public void testDBIndexBaz() throws Exception
   {
      boolean found = true;

      // Column BAZ should not be indexed
      found = lookForIndexOnTable("DBINDEXTEST", "BAZ");
      assertTrue("Error: column baz is indexed but shouldn't", found == false);

   }

   public void testUserSQL1() throws Exception
   {
      boolean found = false;

      // Column BAR should be indexed by the <post-table-create/>
      // setting
      found = lookForIndexOnTable("DBINDEXTEST", "BAR");
      assertTrue("Error column bar is not indexed via post-table-create", found);
   }

   // 
   // Check the "default" case for CMR1- and CMR2-Bean, which is
   //  foo* has an index and bar* has no index

   public void testCMR1IndexFoo() throws Exception
   {
      boolean found = false;

      found = lookForIndexOnTable("CMR1", "FOO1");
      assertTrue("Error: column foo1 is not indexed", found);
   }

   public void testCMR1IndexBar() throws Exception
   {
      boolean found = true;

      found = lookForIndexOnTable("CMR1", "BAR1");
      assertTrue("Error: column bar1 is indexed and shouldn't", found == false);
   }

   public void testCMR2IndexFoo() throws Exception
   {
      boolean found = false;

      found = lookForIndexOnTable("CMR2", "FOO2");
      assertTrue("Error: column foo1 is not indexed", found);
   }

   public void testCMR2IndexBar() throws Exception
   {
      boolean found = true;

      found = lookForIndexOnTable("CMR2", "BAR2");
      assertTrue("Eror: column bar2 is indexed and shouldn't", found == false);
   }

   //
   // Look for index on fk-mapping in 1:n case
   //
   public void testFkIdxCmr1() throws Exception
   {
      boolean found = false;

      found = lookForIndexOnTable("CMR1", "IDXFK");
      assertTrue("Error: Foreign key idxFk is not indexed", found);
   }

   /*
    * Look for indices on the m:n mapping table
    * This is for hsql a strange case, at indices are put there
    * anyway, but it has been told that other databases don't do
    * it by themselves, so we check if the creation succeeds.
    */

   public void testCMRmn1() throws Exception
   {
      boolean found = false;

      found = lookForIndexOnTable("IDX_CMR2_REL", "IDX_ID");
      assertTrue("Error: FKey idx_id is not indexed via <dbindex>", found);
   }

   public void testCMRmn2() throws Exception
   {
      boolean found = true;

      found = lookForIndexOnTable("IDX_CMR2_REL", "CMR2_ID");
      assertTrue("Error: FKey cmr2_id is indexed", found == false);
   }


   /*
    * Test if the replacement of %%t and %%n in <post-table-create> worked
    */
    
	public void testDefault1() throws Exception 
	{
		boolean found = true;
       
        // This one should not have this index, as the <default>
        // <post-table-create> is overwritten by a 'bean local'
        // <post-table-create>
		found = lookForIndexOnTable("DBINDXTEST","BLABLA");
		assertTrue("Error: Field blabla in table dbindex has an index",found==false);	
	}
	public void testDefault2() throws Exception 
	{
		boolean found = false;
       
		found = lookForIndexOnTable("CMR1","BLABLA");
		assertTrue("Error: Field blabla in table cmr1 has no index",found);	
	}
	public void testDefault3() throws Exception 
	{
		boolean found = false;
       
		found = lookForIndexOnTable("CMR2","BLABLA");
		assertTrue("Error: Field blabla in table cmr2 has no index",found);	
	}
    
    
    
    
    
   /**
    * Find out if the named column in the given table has an index
    * other than the one (possibly) created by a (composite) primary key 
    */
   private boolean lookForIndexOnTable(String table, String column)
      throws Exception
   {
      boolean found = false;
      Connection cConn = getConnection();
      DatabaseMetaData dMeta = cConn.getMetaData();

      ResultSet rs = dMeta.getIndexInfo(null, null, table, false, false);

      while (rs.next())
      {
         String cName = rs.getString(9); // name of column
         String iName = rs.getString(6); // name of index

         //	Ignore indices that are autogenerated for PKs
         if (cName.equals(column) && !iName.startsWith("PK_IDX"))
         {  					
            found = true;
         }
      }
      rs.close();
      cConn.close();

      return found;
   }
   
   /**
    * Get a JDBC connection from the default datasource, which is
    * expected to be hsqldb
    */
   private Connection getConnection() throws Exception
   {
      Connection cConn;

      InitialContext ic = new InitialContext();
      DataSource ds;
      try
      {
         Object o = ic.lookup("java:/DefaultDS");
         if (o == null)
            log.error("java:/DefaultDS not found");
         ds = (DataSource) o;
      }
      catch (NamingException e)
      {
         log.error("Problem looking up Datasource  " + e.toString());
         throw e;
      }

      cConn = ds.getConnection();
      return cConn;
   }

}
