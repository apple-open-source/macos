
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.test;

import javax.naming.InitialContext;

import org.jboss.test.JBossTestCase;
import junit.framework.*;
import org.jboss.test.jmx.eardeployment.a.interfaces.SessionAHome;
import org.jboss.test.jmx.eardeployment.a.interfaces.SessionA;
import org.jboss.test.jmx.eardeployment.b.interfaces.SessionBHome;
import org.jboss.test.jmx.eardeployment.b.interfaces.SessionB;



/**
 * EarDeploymentUnitTestCase.java
 *
 *
 * Created: Thu Feb 21 20:54:55 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class EarDeploymentUnitTestCase extends JBossTestCase 
{
   public EarDeploymentUnitTestCase(String name)
   {
      super(name);
   }

   /**
    * The <code>testEarSubpackageVisibility</code> method tests if the classes in
    * subpackages of an ear are visible to each other when ejb's are deployed.
    * SessionA and SessionB are in different jars, and each refer to the other.
    * 
    *
    * @exception Exception if an error occurs
    */
   public void testEarSubpackageVisibility() throws Exception
   {
      SessionAHome aHome = (SessionAHome)getInitialContext().lookup("eardeployment/SessionA");
      SessionBHome bHome = (SessionBHome)getInitialContext().lookup("eardeployment/SessionB");
      SessionA a = aHome.create();
      SessionB b = bHome.create();
      assertTrue("a call b failed!", a.callB());
      assertTrue("b call a failed!", b.callA());
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(EarDeploymentUnitTestCase.class, "eardeployment.ear");
   }

   
}// EarDeploymentUnitTestCase
