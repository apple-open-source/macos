/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.enum.test;

import net.sourceforge.junitejb.EJBTestCase;
import junit.framework.Test;
import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.enum.ejb.Facade;
import org.jboss.test.cmp2.enum.ejb.FacadeUtil;
import org.jboss.test.cmp2.enum.ejb.ColorEnum;
import org.jboss.test.cmp2.enum.ejb.AnimalEnum;

/**
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 * @author <a href="mailto:gturner@unzane.com">Gerald Turner</a>
 */
public class EnumUnitTestCase
   extends EJBTestCase
{
   public static Test suite() throws Exception
   {
      return JBossTestCase.getDeploySetup(EnumUnitTestCase.class, "cmp2-enum.jar");
   }

   public EnumUnitTestCase(String s)
   {
      super(s);
   }

   // Tests

   public void testColorEnum()
      throws Exception
   {
      Facade facade = FacadeUtil.getHome().create();
      Long childId = new Long(1);
      facade.createChild(childId);
      assertTrue(ColorEnum.RED == facade.getColorForId(childId));
      facade.setColor(childId, ColorEnum.GREEN);
      assertTrue(ColorEnum.GREEN == facade.getColorForId(childId));
      facade.setColor(childId, ColorEnum.BLUE);
      assertTrue(ColorEnum.BLUE == facade.getColorForId(childId));
      facade.removeChild(childId);
   }

   public void testAnimalEnum()
      throws Exception
   {
      Facade facade = FacadeUtil.getHome().create();
      Long childId = new Long(2);
      facade.createChild(childId);
      assertTrue(AnimalEnum.PENGUIN == facade.getAnimalForId(childId));
      facade.setAnimal(childId, AnimalEnum.DOG);
      assertTrue(AnimalEnum.DOG == facade.getAnimalForId(childId));
      facade.setAnimal(childId, AnimalEnum.CAT);
      assertTrue(AnimalEnum.CAT == facade.getAnimalForId(childId));
      facade.removeChild(childId);
   }
}
