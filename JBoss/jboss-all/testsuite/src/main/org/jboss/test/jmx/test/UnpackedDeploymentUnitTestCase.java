/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jmx.test;

import javax.naming.InitialContext;

import org.jboss.test.jmx.eardeployment.a.interfaces.SessionAHome;
import org.jboss.test.jmx.eardeployment.a.interfaces.SessionA;
import org.jboss.test.jmx.eardeployment.b.interfaces.SessionBHome;
import org.jboss.test.jmx.eardeployment.b.interfaces.SessionB;

import org.jboss.test.JBossTestCase;

/** Tests of unpacked deployments.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public class UnpackedDeploymentUnitTestCase extends JBossTestCase 
{
   public UnpackedDeploymentUnitTestCase(String name)
   {
      super(name);
   }

   /** Test that an unpacked ejb-jar is deployed
    * @exception Exception if an error occurs
    */
   public void testUnpackedEjbJar() throws Exception
   {
      deploy("unpacked/eardeployment.ear");
      try
      {
         SessionAHome aHome = (SessionAHome)getInitialContext().lookup("eardeployment/SessionA");
         SessionBHome bHome = (SessionBHome)getInitialContext().lookup("eardeployment/SessionB");
         SessionA a = aHome.create();
         SessionB b = bHome.create();
         assertTrue("a call b failed!", a.callB());
         assertTrue("b call a failed!", b.callA());
      }
      finally
      {
         undeploy("unpacked/eardeployment.ear");
      }
   }
  
}

