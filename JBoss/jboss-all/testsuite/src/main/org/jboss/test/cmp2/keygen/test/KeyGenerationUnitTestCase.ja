/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.test.cmp2.keygen.test;

import java.util.Collection;
import javax.naming.Context;
import javax.naming.InitialContext;

import junit.framework.Test;
import net.sourceforge.junitejb.EJBTestCase;
import org.apache.log4j.Logger;
import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.keygen.ejb.UnknownPKLocal;
import org.jboss.test.cmp2.keygen.ejb.UnknownPKLocalHome;
import org.jboss.test.cmp2.keygen.ejb.IntegerPKLocalHome;

/** Tests of the entity-command key generation
 *
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.5 $
 */
public class KeyGenerationUnitTestCase extends EJBTestCase
{
   static final Logger log = Logger.getLogger(KeyGenerationUnitTestCase.class);

   public static Test suite() throws Exception
   {
      return JBossTestCase.getDeploySetup(KeyGenerationUnitTestCase.class, "cmp2-keygen.jar");
   }

   public KeyGenerationUnitTestCase(String name)
   {
      super(name);
   }

   public void setUpEJB() throws Exception
   {
      super.setUpEJB();
   }

   public void testUUIDKeyGenerator() throws Exception
   {
      UnknownPKLocalHome home = getUnknownPKHome("local/TestUUIDKeyGenEJB");
      UnknownPKLocal ejb1 = home.create("testUUIDKeyGenerator");
      UnknownPKLocal ejb2 = home.create("testUUIDKeyGenerator");
      try
      {
         UnknownPKLocal ejb1a = home.findByPrimaryKey(ejb1.getPrimaryKey());
         assertTrue(ejb1.isIdentical(ejb1a));
         assertTrue(ejb1.isIdentical(ejb2) == false);
         assertTrue(ejb1.getPrimaryKey().equals(ejb2.getPrimaryKey()) == false);
      }
      finally
      {
         ejb1.remove();
         ejb2.remove();
      }
   }

   public void testPkSQLKeyGenerator() throws Exception
   {
      UnknownPKLocalHome home = getUnknownPKHome("local/TestPkSqlEJB");
      UnknownPKLocal ejb1 = home.create("testPkSQLKeyGenerator");
      Thread.sleep(50);
      UnknownPKLocal ejb2 = home.create("testPkSQLKeyGenerator");
      try
      {
         UnknownPKLocal ejb1a = home.findByPrimaryKey(ejb1.getPrimaryKey());
         assertTrue(ejb1.isIdentical(ejb1a));
         assertTrue(ejb1.isIdentical(ejb2) == false);
         assertTrue(ejb1.getPrimaryKey().equals(ejb2.getPrimaryKey()) == false);
      }
      finally
      {
         ejb1.remove();
         ejb2.remove();
      }
   }

   public void testHsqldbKeyGenerator() throws Exception
   {
      UnknownPKLocalHome home = getUnknownPKHome("local/TestHsqldbEJB");
      UnknownPKLocal ejb1 = home.create("testHsqldbKeyGenerator");
      UnknownPKLocal ejb2 = home.create("testHsqldbKeyGenerator");
      try
      {
         UnknownPKLocal ejb1a = home.findByPrimaryKey(ejb1.getPrimaryKey());
         assertTrue(ejb1.isIdentical(ejb1a));
         assertTrue(ejb1.isIdentical(ejb2) == false);
         assertTrue(ejb1.getPrimaryKey().equals(ejb2.getPrimaryKey()) == false);
      }
      finally
      {
         ejb1.remove();
         ejb2.remove();
      }
   }

   public void testHsqldbIntegerKeyGenerator() throws Exception
   {
      Context ctx = new InitialContext();
      IntegerPKLocalHome home = (IntegerPKLocalHome) ctx.lookup("local/TestHsqldbIntegerEJB");
      UnknownPKLocal ejb1 = home.create("testHsqldbIntegerKeyGenerator");
      UnknownPKLocal ejb2 = home.create("testHsqldbIntegerKeyGenerator");
      try
      {
         Integer key = (Integer) ejb1.getPrimaryKey();
         UnknownPKLocal ejb1a = home.findByPrimaryKey(key);
         assertTrue(ejb1.isIdentical(ejb1a));
         assertTrue(ejb1.isIdentical(ejb2) == false);
         assertTrue(ejb1.getPrimaryKey().equals(ejb2.getPrimaryKey()) == false);
      }
      finally
      {
         ejb1.remove();
         ejb2.remove();
      }
   }

   public void testInvalidHsqldbIntegerKeyGenerator() throws Exception
   {
      Context ctx = new InitialContext();
      IntegerPKLocalHome home = (IntegerPKLocalHome) ctx.lookup("local/InvalidHsqldbIntegerEJB");
      try
      {
         UnknownPKLocal ejb1 = home.create("testInvalidHsqldbIntegerKeyGenerator");
         Object key = ejb1.getPrimaryKey();
         assertTrue("InvalidHsqldbIntegerEJB key != null", key != null);
      }
      catch(Exception e)
      {
         log.debug("create failed as expected", e);
         // Remove the bean that was inserted into the table
         Collection beans = home.findAll();
         UnknownPKLocal ejb1 = (UnknownPKLocal) beans.iterator().next();
         ejb1.remove();
      }
   }

   public void testOtherKeyGenerator() throws Exception
   {
      UnknownPKLocalHome home = getUnknownPKHome("local/TestOtherEJB");
      UnknownPKLocal ejb1 = home.create("testOtherKeyGenerator1");
      UnknownPKLocal ejb2 = home.create("testOtherKeyGenerator2");
      try
      {
         UnknownPKLocal ejb1a = home.findByPrimaryKey(ejb1.getPrimaryKey());
         assertTrue(ejb1.isIdentical(ejb1a));
         assertEquals("testOtherKeyGenerator1", ejb1a.getValue());
         assertTrue(ejb1.isIdentical(ejb2) == false);
         assertTrue(ejb1.getPrimaryKey().equals(ejb2.getPrimaryKey()) == false);
      }
      finally
      {
         ejb1.remove();
         ejb2.remove();
      }
   }

   private UnknownPKLocalHome getUnknownPKHome(String jndiName) throws Exception
   {
      Context ctx = new InitialContext();
      return (UnknownPKLocalHome) ctx.lookup(jndiName);
   }
}
