/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jrmp.test;

import java.io.IOException;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

import org.jboss.test.jrmp.interfaces.StatelessSession;
import org.jboss.test.jrmp.interfaces.StatelessSessionHome;

/**
 * Test of using custom RMI socket factories with the JRMP ejb container
 * invoker.
 *
 * @author    Scott_Stark@displayscape.com
 * @author    david jencks d_jencks@users.sourceforge.net
 * @version   $Revision: 1.6 $
 */
public class CustomSocketsUnitTestCase extends JBossTestCase
{
   /**
    * Constructor for the CustomSocketsUnitTestCase object
    *
    * @param name  Description of Parameter
    */
   public CustomSocketsUnitTestCase(String name)
   {
      super(name);
   }


   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testCustomAccess() throws Exception
   {
      InitialContext jndiContext = new InitialContext();
      getLog().debug("Lookup StatelessSession");
      Object obj = jndiContext.lookup("StatelessSession");
      StatelessSessionHome home = (StatelessSessionHome)obj;
      getLog().debug("Found StatelessSession Home");
      StatelessSession bean = home.create();
      getLog().debug("Created StatelessSession");
      // Test that the Entity bean sees username as its principal
      String echo = bean.echo("jrmp-comp");
      getLog().debug("bean.echo(jrmp-comp) = " + echo);
      bean.remove();
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testAccess() throws Exception
   {
      InitialContext jndiContext = new InitialContext();
      getLog().debug("Lookup StatefulSession");
      Object obj = jndiContext.lookup("StatefulSession");
      StatelessSessionHome home = (StatelessSessionHome)obj;
      getLog().debug("Found StatefulSession Home");
      StatelessSession bean = home.create();
      getLog().debug("Created StatefulSession");
      // Test that the Entity bean sees username as its principal
      String echo = bean.echo("jrmp");
      getLog().debug("bean.echo(jrmp) = " + echo);
      bean.remove();
   }


   public static Test suite() throws Exception
   {
      return getDeploySetup(CustomSocketsUnitTestCase.class, "jrmp-comp.jar");
   }

}
