
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.cmp2.optimisticlock.test;

import javax.naming.InitialContext;
import javax.naming.NamingException;

import javax.rmi.PortableRemoteObject;

import junit.framework.Test;
import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.optimisticlock.interfaces.FacadeHome;
import org.jboss.test.cmp2.optimisticlock.interfaces.Facade;


/**
 * This class tests optimistic locking with different strategies.
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 */
public class OptimisticLockUnitTestCase
   extends JBossTestCase
{
   // Constants -------------------------------------
   private static final String ENTITY_GROUP_LOCKING = "local/EntityGroupLocking";
   private static final String ENTITY_MODIFIED_LOCKING = "local/EntityModifiedLocking";
   private static final String ENTITY_READ_LOCKING = "local/EntityReadLocking";
   private static final String ENTITY_VERSION_LOCKING = "local/EntityVersionLocking";
   private static final String ENTITY_EXPLICIT_VERSION_LOCKING = "local/EntityExplicitVersionLocking";
   private static final String ENTITY_TIMESTAMP_LOCKING = "local/EntityTimestampLocking";
   private static final String ENTITY_KEYGEN_LOCKING = "local/EntityKeyGeneratorLocking";

   // Attributes ------------------------------------
   private FacadeHome facadeHome;
   /** entity primary key value */
   private static final Integer id = new Integer(1);

   // Constructor -----------------------------------
   public OptimisticLockUnitTestCase(String name)
   {
      super(name);
   }

   // TestCase overrides ----------------------------
   public static Test suite() throws Exception
   {
      return JBossTestCase.getDeploySetup(
         OptimisticLockUnitTestCase.class, "cmp2-optimisticlock.jar");
   }

   // Tests -----------------------------------------
   public void testNullLockedFields() throws Exception
   {
      Facade facade = getFacadeHome().create();
      facade.createCmpEntity(ENTITY_MODIFIED_LOCKING, id,
         null, new Integer(1), null, "str2", null, new Double(2.2));
      try
      {
         facade.testNullLockedFields(ENTITY_MODIFIED_LOCKING, id);
      }
      finally
      {
         tearDown(ENTITY_MODIFIED_LOCKING);
      }
   }

   public void testKeygenStrategyPass() throws Exception
   {
      setup(ENTITY_KEYGEN_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testKeygenStrategyPass(ENTITY_KEYGEN_LOCKING, id);
      }
      finally
      {
         tearDown(ENTITY_KEYGEN_LOCKING);
      }
   }

   public void testKeygenStrategyFail() throws Exception
   {
      setup(ENTITY_KEYGEN_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testKeygenStrategyFail(ENTITY_KEYGEN_LOCKING, id);
         fail("Should have failed to update.");
      }
      catch(Exception e) {}
      finally
      {
         tearDown(ENTITY_KEYGEN_LOCKING);
      }
   }

   public void testTimestampStrategyPass() throws Exception
   {
      setup(ENTITY_TIMESTAMP_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testTimestampStrategyPass(ENTITY_TIMESTAMP_LOCKING, id);
      }
      finally
      {
         tearDown(ENTITY_TIMESTAMP_LOCKING);
      }
   }

   public void testTimestampStrategyFail() throws Exception
   {
      setup(ENTITY_TIMESTAMP_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testTimestampStrategyFail(ENTITY_TIMESTAMP_LOCKING, id);
         fail("Should have failed to update.");
      }
      catch(Exception e) {}
      finally
      {
         tearDown(ENTITY_TIMESTAMP_LOCKING);
      }
   }

   public void testVersionStrategyPass() throws Exception
   {
      setup(ENTITY_VERSION_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testVersionStrategyPass(ENTITY_VERSION_LOCKING, id);
      }
      finally
      {
         tearDown(ENTITY_VERSION_LOCKING);
      }
   }

   public void testVerionStrategyFail() throws Exception
   {
      setup(ENTITY_VERSION_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testVersionStrategyFail(ENTITY_VERSION_LOCKING, id);
         fail("Should have failed to update.");
      }
      catch(Exception e) {}
      finally
      {
         tearDown(ENTITY_VERSION_LOCKING);
      }
   }

   public void testExplicitVersionStrategyPass() throws Exception
   {
      setup(ENTITY_EXPLICIT_VERSION_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testVersionStrategyPass(ENTITY_EXPLICIT_VERSION_LOCKING, id);
      }
      finally
      {
         tearDown(ENTITY_EXPLICIT_VERSION_LOCKING);
      }
   }

   public void testExplicitVerionStrategyFail() throws Exception
   {
      setup(ENTITY_EXPLICIT_VERSION_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testVersionStrategyFail(ENTITY_EXPLICIT_VERSION_LOCKING, id);
         fail("Should have failed to update.");
      }
      catch(Exception e) {}
      finally
      {
         tearDown(ENTITY_EXPLICIT_VERSION_LOCKING);
      }
   }

   public void testExplicitVersionUpdateOnSync() throws Exception
   {
      setup(ENTITY_EXPLICIT_VERSION_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testExplicitVersionUpdateOnSync(ENTITY_EXPLICIT_VERSION_LOCKING, id);
      }
      catch(Exception e)
      {
         fail("Locked fields are not updated on sync!");
      }
      finally
      {
         tearDown(ENTITY_EXPLICIT_VERSION_LOCKING);
      }
   }

   public void testGroupStrategyPass() throws Exception
   {
      setup(ENTITY_GROUP_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testGroupStrategyPass(ENTITY_GROUP_LOCKING, id);
      }
      finally
      {
         tearDown(ENTITY_GROUP_LOCKING);
      }
   }

   public void testGroupStrategyFail() throws Exception
   {
      setup(ENTITY_GROUP_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testGroupStrategyFail(ENTITY_GROUP_LOCKING, id);
         fail("Should have failed to update!");
      }
      catch(Exception e) {}
      finally
      {
         tearDown(ENTITY_GROUP_LOCKING);
      }
   }

   public void testReadStrategyPass() throws Exception
   {
      setup(ENTITY_READ_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testReadStrategyPass(ENTITY_READ_LOCKING, id);
      }
      finally
      {
         tearDown(ENTITY_READ_LOCKING);
      }
   }

   public void testReadStrategyFail() throws Exception
   {
      setup(ENTITY_READ_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testReadStrategyFail(ENTITY_READ_LOCKING, id);
         fail("Should have failed to update.");
      }
      catch(Exception e) {}
      finally
      {
         tearDown(ENTITY_READ_LOCKING);
      }
   }

   public void testModifiedStrategyPass() throws Exception
   {
      setup(ENTITY_MODIFIED_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testModifiedStrategyPass(ENTITY_MODIFIED_LOCKING, id);
      }
      finally
      {
         tearDown(ENTITY_MODIFIED_LOCKING);
      }
   }

   public void testModifiedStrategyFail() throws Exception
   {
      setup(ENTITY_MODIFIED_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testModifiedStrategyFail(ENTITY_MODIFIED_LOCKING, id);
         fail("Should have failed to update!");
      }
      catch(Exception e)
      {
         // expected
      }
      finally
      {
         tearDown(ENTITY_MODIFIED_LOCKING);
      }
   }

   public void testUpdateLockOnSync() throws Exception
   {
      setup(ENTITY_VERSION_LOCKING);
      Facade facade = getFacadeHome().create();
      try
      {
         facade.testUpdateLockOnSync(ENTITY_VERSION_LOCKING, id);
      }
      catch(Exception e)
      {
         fail("Locked fields are not updated on sync!");
      }
      finally
      {
         tearDown(ENTITY_VERSION_LOCKING);
      }
   }

   // Private

   private void setup(String jndiName) throws Exception
   {
      Facade facade = getFacadeHome().create();
      facade.createCmpEntity(jndiName, id,
         "str1", new Integer(1), new Double(1.1),
         "str2", new Integer(2), new Double(2.2));
   }

   private void tearDown(String jndiName) throws Exception
   {
      Facade facade = getFacadeHome().create();
      facade.safeRemove(jndiName, id);
   }

   private FacadeHome getFacadeHome()
      throws NamingException
   {
      if(facadeHome == null)
      {
         InitialContext ic = new InitialContext();
         Object ref = ic.lookup(FacadeHome.JNDI_NAME);
         facadeHome = (FacadeHome)PortableRemoteObject.narrow(
            ref, FacadeHome.class
         );
      }
      return facadeHome;
   }
}
