/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.test;

import javax.naming.Context;

import org.jboss.test.cts.interfaces.StatefulSession;
import org.jboss.test.cts.interfaces.StatefulSessionHome;
import org.jboss.test.JBossTestCase;

import junit.framework.Test;

/**
 * Long wait test
 *
 * @author adrian@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class LongWaitStatefulSessionUnitTestCase
   extends JBossTestCase
{
   public LongWaitStatefulSessionUnitTestCase (String name)
   {
      super(name);
   }

	/** 
     * Invoke a bean that waits for longer than the passivation time
     */
	public void testLongWait() throws Exception
	{
      Context ctx = getInitialContext();
      getLog().debug("+++ testLongWait");
      StatefulSessionHome sessionHome = ( StatefulSessionHome ) ctx.lookup("ejbcts/LongWaitStatefulSessionBean");
      StatefulSession sessionBean = sessionHome.create("testLongWait");		

      getLog().debug("Sleeping...");
      sessionBean.sleep(5000);
      sessionBean.ping();
      getLog().debug("+++ testLongWait passed");
	}

   public static Test suite() throws Exception
   {
      return getDeploySetup(LongWaitStatefulSessionUnitTestCase.class, "cts.jar");
   }

}
