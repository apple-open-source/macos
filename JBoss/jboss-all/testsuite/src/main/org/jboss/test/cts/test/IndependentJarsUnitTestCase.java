/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.test;

import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.cts.interfaces.CallerSession;
import org.jboss.test.cts.interfaces.CallerSessionHome;
import org.jboss.test.cts.interfaces.CalleeException;

/** Tests of ejbs in seperate jars interacting
 *
 *  @author Scott.Stark@jboss.org
 *  @version $Revision: 1.1.2.3 $
 */
public class IndependentJarsUnitTestCase
      extends JBossTestCase
{
   CallerSession sessionBean;

   public IndependentJarsUnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      InitialContext ctx = new InitialContext();
      Object ref = ctx.lookup("ejbcts/CallerSessionHome");
      CallerSessionHome home = (CallerSessionHome)
            PortableRemoteObject.narrow(ref, CallerSessionHome.class);
      sessionBean = home.create();
   }

   protected void tearDown() throws Exception
   {
      if (sessionBean != null)
         sessionBean.remove();
   }


   /** A test of two ejb deployments cts.jar and cts2.jar(scoped) with an EJB
    * in cts.jar calling an EJB in cts2.jar. The cts2.jar EJB is deployed with
    * the ByValueInvokerInterceptor to isolate the two type namespaces.
    * @throws Exception
    */
   public void testInterJarCall() throws Exception
   {
      // Deploy the cts2.jar
      deploy("cts2.jar");
      sessionBean.simpleCall(true);
      sessionBean.simpleCall2(true);
      try
      {
         sessionBean.callAppEx();
      }
      catch(CalleeException e)
      {
         log.info("Saw excpected exception", e);
      }
      undeploy("cts2.jar");
      deploy("cts2.jar");
      sessionBean.simpleCall(true);
      sessionBean.simpleCall2(true);
      try
      {
         sessionBean.callAppEx();
      }
      catch(CalleeException e)
      {
         log.info("Saw excpected exception", e);
      }
      undeploy("cts2.jar");
   }

   /** A test of two ejb deployments cts.jar and cts2.jar(scoped) with an EJB
    * in cts.jar calling an EJB in cts2.jar. The cts2.jar EJB is deployed with
    * the ByValueInvokerInterceptor to isolate the two type namespaces.
    * @throws Exception
    */
   public void testCallByValueInSameJar() throws Exception
   {
      sessionBean.callByValueInSameJar();
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(IndependentJarsUnitTestCase.class, "cts.jar");
   }

}
