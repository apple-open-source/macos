/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jca.test;

import junit.framework.*;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jca.interfaces.UserTxSessionHome;
import org.jboss.test.jca.interfaces.UserTxSession;

/**
 * UserTxUnitTestCase.java
 *
 *
 * Created: Thu Jun 27 09:38:10 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class UserTxUnitTestCase extends JBossTestCase
{
   private UserTxSessionHome sh;
   private UserTxSession s;


   public UserTxUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite() throws Exception
   {
      Test t1 = getDeploySetup(UserTxUnitTestCase.class, "jcatest.jar");
      Test t2 = getDeploySetup(t1, "testadapter-ds.xml");
      return getDeploySetup(t2, "jbosstestadapter.rar");
   }

   protected void setUp() throws Exception
   {
      sh = (UserTxSessionHome)getInitialContext().lookup("UserTxSession");
      s = sh.create();
   }

   public void testUserTxJndi() throws Exception
   {
      assertTrue("Not enrolled in Tx!", s.testUserTxJndi());
   }

   public void testUserTxSessionCtx() throws Exception
   {
      assertTrue("Not enrolled in Tx!", s.testUserTxSessionCtx());
   }

}// UserTxUnitTestCase
