/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.entity.test;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;

import org.jboss.test.entity.interfaces.TestEntity;
import org.jboss.test.entity.interfaces.TestEntityHome;
import org.jboss.test.entity.interfaces.TestEntityUtil;
import org.jboss.test.entity.interfaces.TestEntityValue;

/**
 * Some entity bean tests.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.2.2.3 $
 */
public class EntityUnitTestCase
   extends JBossTestCase
{
   public EntityUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite()
      throws Exception
   {
      return getDeploySetup(EntityUnitTestCase.class, "jboss-test-entity.jar");
   }

   public void testExternalRemoveAfterCreateThenRecreate()
      throws Exception
   {
      getLog().debug("Retrieving home");
      TestEntityHome home = TestEntityUtil.getHome();

      getLog().debug("Creating entity");
      TestEntityValue value = new TestEntityValue("key1", null);
      home.create(value);

      getLog().debug("Removing entity externally");
      home.removeExternal("key1");

      getLog().debug("Recreating the entity");
      home.create(value);
   }

   public void testInternalHomeRemoveAfterCreateThenRecreate()
      throws Exception
   {
      getLog().debug("Retrieving home");
      TestEntityHome home = TestEntityUtil.getHome();

      getLog().debug("Creating entity");
      TestEntityValue value = new TestEntityValue("key2", null);
      home.create(value);

      getLog().debug("Removing entity internally");
      home.remove("key2");

      getLog().debug("Recreating the entity");
      home.create(value);
   }

   public void testInternalBeanRemoveAfterCreateThenRecreate()
      throws Exception
   {
      getLog().debug("Retrieving home");
      TestEntityHome home = TestEntityUtil.getHome();

      getLog().debug("Creating entity");
      TestEntityValue value = new TestEntityValue("key3", null);
      TestEntity bean = home.create(value);

      getLog().debug("Removing entity internally");
      bean.remove();

      getLog().debug("Recreating the entity");
      home.create(value);
   }

   public void testChangeReadOnlyField()
      throws Exception
   {
      getLog().debug("Retrieving home");
      TestEntityHome home = TestEntityUtil.getHome();

      getLog().debug("Creating entity");
      TestEntityValue value = new TestEntityValue("key4", "original");
      TestEntity bean = home.create(value);

      getLog().debug("Access the value");
      assertEquals("original", bean.getValue1());

      getLog().debug("Change the value");
      home.changeValue1("key4", "changed");
      assertEquals("changed", bean.getValue1());
   }
}
