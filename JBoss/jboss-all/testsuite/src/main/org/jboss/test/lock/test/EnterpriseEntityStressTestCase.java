/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.lock.test;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

/**
 * #Description of the Class
 */
public class EnterpriseEntityStressTestCase
       extends JBossTestCase
{
   /**
    * Constructor for the EnterpriseEntityStressTestCase object
    *
    * @param name  Description of Parameter
    */
   public EnterpriseEntityStressTestCase(String name)
   {
      super(name);
   }

   /**
    * Setup the test suite.
    *
    * @return   The test suite
    */
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();


      // Test ejb.plugins.lock.QueuedPessimisticEJBLock
      suite.addTest(new TestSuite(Entity_Option_A_Test.class));
      suite.addTest(new TestSuite(Entity_Option_B_Test.class));
      suite.addTest(new TestSuite(Entity_Option_C_Test.class));
      suite.addTest(new TestSuite(Entity_Option_D_Test.class));

      suite.addTest(new TestSuite(Entity_Option_C_Multi_Test.class));

      return getDeploySetup(suite, "locktest.jar");
   }

   /**
    * #Description of the Class
    */
   public static class Entity_Option_A_Test
          extends EnterpriseEntityTest
   {
      /**
       * Constructor for the Entity_Option_A_Test object
       *
       * @param name  Description of Parameter
       */
      public Entity_Option_A_Test(String name)
      {
         super(name, "EnterpriseEntity_A");
      }
   }

   /**
    * #Description of the Class
    */
   public static class Entity_Option_B_Test
          extends EnterpriseEntityTest
   {
      /**
       * Constructor for the Entity_Option_B_Test object
       *
       * @param name  Description of Parameter
       */
      public Entity_Option_B_Test(String name)
      {
         super(name, "EnterpriseEntity_B");
      }
      
      public void testB2B() throws Exception
      {
         // This test will not work with commit-option B, because
         // all fields of the entity bean are nulled out on activation
      }
      
   }

   /**
    * #Description of the Class
    */
   public static class Entity_Option_C_Test
          extends EnterpriseEntityTest
   {
      /**
       * Constructor for the Entity_Option_C_Test object
       *
       * @param name  Description of Parameter
       */
      public Entity_Option_C_Test(String name)
      {
         super(name, "EnterpriseEntity_C");
      }
      public void testB2B() throws Exception
      {
         // This test will not work with commit-option C, because
         // all fields of the entity bean are nulled out on activation
      }
   }

   /**
    * #Description of the Class
    */
   public static class Entity_Option_D_Test
          extends EnterpriseEntityTest
   {
      /**
       * Constructor for the Entity_Option_D_Test object
       *
       * @param name  Description of Parameter
       */
      public Entity_Option_D_Test(String name)
      {
         super(name, "EnterpriseEntity_D");
      }
   }

   /**
    * #Description of the Class
    */
   public static class Entity_Option_B_Multi_Test
          extends EnterpriseEntityTest
   {
      /**
       * Constructor for the Entity_Option_B_Multi_Test object
       *
       * @param name  Description of Parameter
       */
      public Entity_Option_B_Multi_Test(String name)
      {
         super(name, "EnterpriseEntity_B_Multi");
      }
      public void testB2B() throws Exception
      {
         // This test will not work with commit-option B, because
         // all fields of the entity bean are nulled out on activation
      }
   }

   /**
    * #Description of the Class
    */
   public static class Entity_Option_C_Multi_Test
          extends EnterpriseEntityTest
   {
      /**
       * Constructor for the Entity_Option_C_Multi_Test object
       *
       * @param name  Description of Parameter
       */
      public Entity_Option_C_Multi_Test(String name)
      {
         super(name, "EnterpriseEntity_C_Multi");
      }
      public void testB2B() throws Exception
      {
         // This test will not work with commit-option C, because
         // all fields of the entity bean are nulled out on activation
      }
   }

}

