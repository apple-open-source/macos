
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.test;

import javax.naming.InitialContext;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jca.interfaces.Reentrant;
import org.jboss.test.jca.interfaces.ReentrantHome;
/**
 * ReentrantUnitTestCase.java
 *
 *
 * Created: Wed Jul 31 14:30:43 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class ReentrantUnitTestCase extends JBossTestCase {

   public ReentrantUnitTestCase (String name)
   {
      super(name);
   }
   
   public static Test suite() throws Exception
   {
      Test t1 = getDeploySetup(ReentrantUnitTestCase.class, "jcatest.jar");
      Test t2 = getDeploySetup(t1, "testadapter-ds.xml");
      return getDeploySetup(t2, "jbosstestadapter.rar");
   }

   public void testReentrantConnectionCaching() throws Exception
   {
      ReentrantHome rh = (ReentrantHome)new InitialContext().lookup("/ejb/jca/Reentrant");
      Reentrant r1 = rh.create(new Integer(0), null);
      Reentrant r2 = rh.create(new Integer(1), r1);
   }

}// ReentrantUnitTestCase
