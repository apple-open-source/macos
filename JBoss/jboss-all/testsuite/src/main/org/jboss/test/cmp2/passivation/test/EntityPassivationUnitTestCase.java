/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.cmp2.passivation.test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import javax.rmi.PortableRemoteObject;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.passivation.interfaces.RapidlyPassivatedEntity;
import org.jboss.test.cmp2.passivation.interfaces.RapidlyPassivatedEntityHome;

/**
 * Tests the integrity of entity beans after they have been through
 * a passivate/activate cycle.
 * 
 * It has been designed to expose the bug described at
 * <a href="https://sourceforge.net/tracker/?group_id=22866&atid=376685&func=detail&aid=742197">
 *    Detail:769139 entityCtx.getEJBLocalObject() returns wrong instance</a>
 * and
 * <a href="https://sourceforge.net/tracker/?func=detail&atid=376685&aid=769139&group_id=22866">
 *    Detail:742197 getEJBLocalObject()  bug</a>
 *
 */
public class EntityPassivationUnitTestCase extends JBossTestCase
{

   // Constants -----------------------------------------------------
   static final int ENTITY_PASSIVATION_TIMEOUT = 15 * 1000;
   
   // Attributes ----------------------------------------------------
   List mEntities = new ArrayList(getBeanCount());

   // Static --------------------------------------------------------

   public static Test suite()
      throws Exception
   {
      return getDeploySetup(EntityPassivationUnitTestCase.class, "cmp2-passivation.jar");
   }

   // Constructors --------------------------------------------------

   public EntityPassivationUnitTestCase(String name)
   {
      super(name);
   }


   // Public --------------------------------------------------------

   /**
    * Wait for the entities to be passivated and then check their
    * context's idea of which entity is actually referenced.
    * via its local object reference.
    * @throws Exception
    */
   public void testPostPassivationLocalEJBIntegrity()
      throws Exception
   {
      log.info("Waiting for entities to passivate");
      Thread.sleep(ENTITY_PASSIVATION_TIMEOUT);
      for (Iterator i = mEntities.iterator(); i.hasNext();)
      {
         RapidlyPassivatedEntity e = (RapidlyPassivatedEntity)i.next();
         assertEquals(e.getIdViaEJBLocalObject(), e.getPrimaryKey());
      }
   }
   

   /**
    * Wait for the entities to be passivated and then check their
    * context's idea of which entity is actually referenced
    * via its remote object reference.
    * 
    * This probably should be run against a collection of
    * local interfaces in a session bean, rather than
    * here.
    * 
    * TODO Place this test in SLSB with a collection of local interfaces.
    * @throws Exception
    */
   public void testPostPassivationRemoteEJBIntegrity()
      throws Exception
   {
      log.info("Waiting for entities to passivate");
      Thread.sleep(ENTITY_PASSIVATION_TIMEOUT);
      for (Iterator i = mEntities.iterator(); i.hasNext();)
      {
         RapidlyPassivatedEntity e = (RapidlyPassivatedEntity)i.next();
         assertEquals(e.getIdViaEJBObject(), e.getPrimaryKey());
      }
   }
   

   // Protected --------------------------------------------------------

   /**
    * Create some entities to test against.
    * @see junit.framework.TestCase#setUp()
    */
   protected void setUp() throws Exception
   {
      super.setUp();
      Object homeObject =
         getInitialContext().lookup(RapidlyPassivatedEntityHome.JNDI_NAME);
      RapidlyPassivatedEntityHome home =
         (RapidlyPassivatedEntityHome) PortableRemoteObject.narrow(
            homeObject,
            RapidlyPassivatedEntityHome.class);

      for (int i = 0, n = getBeanCount(); i < n; ++i)
         mEntities.add(home.create("nothing to see here"));

   }
}
