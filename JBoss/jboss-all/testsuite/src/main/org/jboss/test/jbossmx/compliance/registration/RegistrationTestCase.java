/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.registration;

import org.jboss.test.jbossmx.compliance.TestCase;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.MBeanRegistrationException;
import javax.management.NotCompliantMBeanException;
import javax.management.InstanceNotFoundException;

import org.jboss.test.jbossmx.compliance.registration.support.RegistrationAware;

public class RegistrationTestCase
   extends TestCase
{
   public RegistrationTestCase(String s)
   {
      super(s);
   }

   public void testSimpleRegistration()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.newMBeanServer();
         RegistrationAware ra = new RegistrationAware();
         ObjectName name = new ObjectName("test:key=value");

         server.registerMBean(ra, name);

         assertTrue("preRegister", ra.isPreRegisterCalled());
         assertTrue("postRegister", ra.isPostRegisterCalled());
         assertTrue("postRegisterRegistrationDone", ra.isPostRegisterRegistrationDone());
         assertEquals(name, ra.getRegisteredObjectName());

         server.unregisterMBean(name);

         assertTrue("preDeRegister", ra.isPreDeRegisterCalled());
         assertTrue("postDeRegister", ra.isPostDeRegisterCalled());
      }
      catch (MalformedObjectNameException e)
      {
         fail("spurious MalformedObjectNameException");
      }
      catch (MBeanRegistrationException e)
      {
         fail("strange MBeanRegistrationException linked to: " + e.getTargetException().getMessage());
      }
      catch (Exception e)
      {
         fail("something else went wrong: " + e.getMessage());
      }
   }

   public void testDuplicateRegistration()
   {

      try
      {
         MBeanServer server = MBeanServerFactory.newMBeanServer();
         ObjectName name = new ObjectName("test:key=value");

         RegistrationAware original = new RegistrationAware();
         RegistrationAware ra = new RegistrationAware();

         server.registerMBean(original, name);

         try
         {
            server.registerMBean(ra, name);
            fail("expected a InstanceAlreadyExistsException");
         }
         catch (InstanceAlreadyExistsException e)
         {
         }

         assertTrue("preRegister", ra.isPreRegisterCalled());
         assertTrue("postRegister", ra.isPostRegisterCalled());
         assertTrue("postRegisterRegistrationDone", !ra.isPostRegisterRegistrationDone());
         assertTrue("preDeRegister", !ra.isPreDeRegisterCalled());
         assertTrue("postDeRegister", !ra.isPostDeRegisterCalled());
         assertEquals(name, ra.getRegisteredObjectName());

         server.unregisterMBean(name);
      }
      catch (Exception e)
      {
         fail("got an unexpected exception: " + e.getMessage());
      }
   }

}
