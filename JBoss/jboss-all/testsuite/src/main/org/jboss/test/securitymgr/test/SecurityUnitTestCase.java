/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.securitymgr.test;

import java.io.IOException;
import javax.naming.InitialContext;

import org.jboss.test.securitymgr.interfaces.Bad;
import org.jboss.test.securitymgr.interfaces.BadHome;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

/** Tests of the security permission enforcement for items outside of the
 standard EJB programming restrictions.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
 */
public class SecurityUnitTestCase extends JBossTestCase
{
   org.apache.log4j.Category log = getLog();

   public SecurityUnitTestCase(String name)
   {
      super(name);
   }

   /** Test that a bean cannot access the SecurityAssociation class
    */
   public void testSecurityAssociation() throws Exception
   {
      log.debug("+++ testSecurityAssociation()");
      Bad bean = getBadSession();

      try
      {
         bean.getSecurityAssociationPrincipal();
         fail("Was able to call Bad.getSecurityAssociationPrincipal");
      }
      catch(Exception e)
      {
         log.debug("Bad.getSecurityAssociationPrincipal failed as expected", e);
      }

      try
      {
         bean.getSecurityAssociationCredential();
         fail("Was able to call Bad.getSecurityAssociationCredential");
      }
      catch(Exception e)
      {
         log.debug("Bad.getSecurityAssociationCredential failed as expected", e);
      }

      try
      {
         bean.setSecurityAssociationPrincipal(null);
         fail("Was able to call Bad.setSecurityAssociationPrincipal");
      }
      catch(Exception e)
      {
         log.debug("Bad.setSecurityAssociationPrincipal failed as expected", e);
      }

      try
      {
         char[] password = "secret".toCharArray();
         bean.setSecurityAssociationCredential(password);
         fail("Was able to call Bad.setSecurityAssociationCredential");
      }
      catch(Exception e)
      {
         log.debug("Bad.setSecurityAssociationCredential failed as expected", e);
      }
      bean.remove();
   }

   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      return getDeploySetup(SecurityUnitTestCase.class, "securitymgr-ejb.jar");
   }

   private Bad getBadSession() throws Exception
   {
      Object obj = getInitialContext().lookup("secmgr.BadHome");
      BadHome home = (BadHome) obj;
      log.debug("Found secmgr.BadHome");
      Bad bean = home.create();
      log.debug("Created Bad");
      return bean;
   }
}
