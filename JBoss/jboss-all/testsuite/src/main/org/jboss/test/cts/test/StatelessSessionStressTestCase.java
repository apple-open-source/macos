/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.test;


import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;
import org.jboss.test.cts.interfaces.ClientCallback;
import org.jboss.test.cts.interfaces.StatelessSession;
import org.jboss.test.cts.interfaces.StatelessSessionHome;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;

/**
 * Class StatelessSessionStressTestCase
 *
 *
 * @author d_jencks converted to JBossTestCase and logging.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.7 $
 */
public class StatelessSessionStressTestCase
   extends JBossTestCase
{
   StatelessSession sessionBean;

   public StatelessSessionStressTestCase (String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      InitialContext ctx = new InitialContext();
      StatelessSessionHome home =
         ( StatelessSessionHome ) ctx.lookup("ejbcts/StatelessSessionHome");
      sessionBean = home.create();
   }
   protected void tearDown() throws Exception
   {
      if( sessionBean != null )
         sessionBean.remove();
   }

   public void testBasicStatelessSession()
      throws Exception
   {
      getLog().debug("+++ testBasicStatelessSession()");
      String result = sessionBean.method1("testBasicStatelessSession");
      // Test response
      assertTrue(result.equals("testBasicStatelessSession"));
      sessionBean.remove();
   }

   public void testClientCallback()
      throws Exception
   {
      getLog().debug("+++ testClientCallback()");
      ClientCallbackImpl callback = new ClientCallbackImpl();
      UnicastRemoteObject.exportObject(callback);
      sessionBean.callbackTest(callback, "testClientCallback");
      // Test callback data
      this.assertTrue(callback.wasCalled());
      UnicastRemoteObject.unexportObject(callback, true);
      sessionBean.remove();
   }

   public void testRuntimeError()
      throws Exception
   {
      getLog().debug("+++ testRuntimeError()");
      try
      {
         sessionBean.npeError();
         fail("npeError should have thrown an exception");
      }
      catch(Exception e)
      {
         getLog().debug("Call threw exception", e);
      }
      sessionBean.remove();
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(StatelessSessionStressTestCase.class, "cts.jar");
   }

}

