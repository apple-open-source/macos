/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.fkupdates.test;

import org.jboss.test.cmp2.fkupdates.ejb.ListFacadeHome;
import org.jboss.test.cmp2.fkupdates.ejb.ListFacade;
import org.jboss.test.cmp2.fkupdates.ejb.ListFacadeUtil;
import org.jboss.test.cmp2.fkupdates.ejb.ListEntityPK;
import org.jboss.test.JBossTestCase;
import net.sourceforge.junitejb.EJBTestCase;
import junit.framework.Test;

import java.util.List;
import java.util.Iterator;

/**
 * The test for unnecessary updates of foreign key fields mapped to
 * CMP fields and CMP fields that have foreign key fields mapped to them.
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class FKUpdatesUnitTestCase extends EJBTestCase
{
   private ListFacadeHome facadeHome = null;

   public FKUpdatesUnitTestCase(String s)
   {
      super(s);
   }

   public static Test suite() throws Exception
   {
      return JBossTestCase.getDeploySetup(FKUpdatesUnitTestCase.class, "cmp2-fkupdates.jar");
   }

   public void setUpEJB() throws Exception
   {
      facadeHome = ListFacadeUtil.getHome();
   }

   // Tests

   public void testCMPFieldsUpdatedOnFKLoad()
      throws Exception
   {
      ListFacade lf = facadeHome.create();
      lf.initData();
      List dirtyFields = lf.loadItems(new ListEntityPK(new Integer(1)));

      if(dirtyFields.isEmpty())
         return;

      StringBuffer sb = new StringBuffer(200);
      sb.append("Not expected dirty fields: ");
      for(Iterator dirtyIter = dirtyFields.iterator(); dirtyIter.hasNext();)
      {
         sb.append(dirtyIter.next()).append(dirtyIter.hasNext() ? ',' : '.');
      }
      fail(sb.toString());
   }
}
