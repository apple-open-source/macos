/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.entity.test;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;

import org.jboss.test.entity.interfaces.EntitySession;
import org.jboss.test.entity.interfaces.EntitySessionHome;

/**
 * Some entity bean tests.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.1 $
 */
public class PathologicalUnitTestCase
   extends JBossTestCase
{
   public PathologicalUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite()
      throws Exception
   {
      return getDeploySetup(PathologicalUnitTestCase.class, "jboss-test-pathological-entity.jar");
   }

   public void testErrorFromEjbCreate()
      throws Exception
   {
      getLog().debug("Retrieving session");
      EntitySession session = getEntitySessionEJB();

      getLog().debug("Testing error from ejbCreate");
      session.createPathological("ejbCreate", true);
   }

   public void testErrorFromRemove()
      throws Exception
   {
      getLog().debug("Retrieving session");
      EntitySession session = getEntitySessionEJB();

      getLog().debug("Creating entity");
      session.createPathological("remove", false);

      getLog().debug("Testing error from remove");
      session.removeHomePathological("remove", true);
   }

   public void testErrorFromEjbRemove()
      throws Exception
   {
      getLog().debug("Retrieving session");
      EntitySession session = getEntitySessionEJB();

      getLog().debug("Creating entity");
      session.createPathological("remove", false);

      getLog().debug("Testing error from remove");
      session.removePathological("remove", true);
   }

   public void testErrorFromFind()
      throws Exception
   {
      getLog().debug("Retrieving session");
      EntitySession session = getEntitySessionEJB();

      getLog().debug("Creating entity");
      session.createPathological("find", false);

      getLog().debug("Testing error from find");
      session.findPathological("find", true);
   }

   public void testErrorFromGet()
      throws Exception
   {
      getLog().debug("Retrieving session");
      EntitySession session = getEntitySessionEJB();

      getLog().debug("Creating entity");
      session.createPathological("get", false);

      getLog().debug("Testing error from get");
      session.getPathological("get", true);
   }

   public void testErrorFromSet()
      throws Exception
   {
      getLog().debug("Retrieving session");
      EntitySession session = getEntitySessionEJB();

      getLog().debug("Creating entity");
      session.createPathological("set", false);

      getLog().debug("Testing error from set");
      session.setPathological("set", true);
   }

   private EntitySession getEntitySessionEJB()
      throws Exception
   {
      return ((EntitySessionHome) getInitialContext().lookup("EntitySessionEJB")).create();
   }
}
