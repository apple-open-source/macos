/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.test;


import java.util.Properties;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.cts.interfaces.CtsCmp;
import org.jboss.test.cts.interfaces.CtsCmpHome;
import org.jboss.test.cts.keys.AccountPK;

/** Basic conformance tests for stateless sessions
 *
 *  @author kimptoc
 *  @author d_jencks converted to JBossTestCase and logging.
 *  @author Scott.Stark@jboss.org
 *  @version $Revision: 1.1.2.2 $
 */
public class CmpUnitTestCase
      extends JBossTestCase
{
   private CtsCmpHome home;

   public CmpUnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      InitialContext ctx = new InitialContext();
      Object ref = ctx.lookup("ejbcts/CMPBean");
      home = (CtsCmpHome) ref;
   }

   protected void tearDown() throws Exception
   {
   }

   /**
    * Method testBasicStatelessSession
    * @throws Exception
    */
   public void testBasicCmp()
         throws Exception
   {
      getLog().debug("+++ testBasicCmp()");
      AccountPK pk = new AccountPK("testBasicCmp");
      CtsCmp bean = home.create(pk, "testBasicCmp unitTest");
      String result = bean.getPersonsName();
      // Test response
      assertTrue(result.equals("testBasicCmp unitTest"));
      bean.remove();
   }

   /** Test of accessing the home interface from the remote interface in an env
    * new InitialContext() will not work.
    * @throws Exception
    */
   public void testHomeFromRemoteNoDefaultJNDI()
         throws Exception
   {
      getLog().debug("+++ testHomeFromRemoteNoDefaultJNDI()");

      // Override the JNDI variables in the System properties
      Properties sysProps = System.getProperties();
      Properties newProps = new Properties(sysProps);
      newProps.setProperty("java.naming.factory.initial", "badFactory");
      newProps.setProperty("java.naming.provider.url", "jnp://badhost:12345");
      System.setProperties(newProps);

      // Do a lookup of the home and create a remote using a custom env
      Properties env = new Properties();
      env.setProperty("java.naming.factory.initial", super.getJndiInitFactory());
      env.setProperty("java.naming.provider.url", super.getJndiURL());
      try
      {
         InitialContext ctx = new InitialContext(env);
         Object ref = ctx.lookup("ejbcts/CMPBean");
         CtsCmpHome home = (CtsCmpHome)
               PortableRemoteObject.narrow(ref, CtsCmpHome.class);
         AccountPK pk1 = new AccountPK("bean1");
         CtsCmp bean1 = home.create(pk1, "testHomeFromRemoteNoDefaultJNDI");
         CtsCmpHome home2 = (CtsCmpHome) bean1.getEJBHome();
         AccountPK pk2 = new AccountPK("bean2");
         CtsCmp bean2 = home2.create(pk2, "testHomeFromRemoteNoDefaultJNDI");
         bean2.remove();
      }
      finally
      {
         System.setProperties(sysProps);
      }
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(CmpUnitTestCase.class, "cts.jar");
   }

}
