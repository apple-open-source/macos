/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.standard;

import junit.framework.TestCase;
import test.compliance.standard.support.Trivial;

import javax.management.InstanceAlreadyExistsException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.MalformedObjectNameException;
import javax.management.NotCompliantMBeanException;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ReflectionException;

/**
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */

public class TrivialTEST extends TestCase
{
   public TrivialTEST(String s)
   {
      super(s);
   }

   public void testRegistration()
   {
      MBeanServer server = MBeanServerFactory.newMBeanServer();
      Trivial trivial = new Trivial();

      ObjectName name = null;
      try
      {
         name = new ObjectName("trivial:key=val");
         ObjectInstance instance = server.registerMBean(trivial, name);
      }
      catch (Exception e)
      {
         fail("registration failed: " + e.getMessage());
      }
      assertTrue("expected server to report it as registered", server.isRegistered(name));
   }

   public void testConstructorInfo()
   {
      MBeanInfo info = getTrivialInfo();

      MBeanConstructorInfo[] constructors = info.getConstructors();
      assertEquals("constructor list length", 1, constructors.length);

      // I really don't feel like reflecting to get the name of the constructor,
      // it should just be the name of the class right?
      assertEquals("constructor name", Trivial.class.getName(), constructors[0].getName());

      MBeanParameterInfo[] params = constructors[0].getSignature();
      assertEquals("constructor signature length", 0, params.length);
   }

   public void testAttributeInfo()
   {
      MBeanInfo info = getTrivialInfo();

      MBeanAttributeInfo[] attributes = info.getAttributes();
      assertEquals("attribute list length", 1, attributes.length);
      assertEquals("attribute name", "Something", attributes[0].getName());
      assertEquals("attribute type", String.class.getName(), attributes[0].getType());
      assertEquals("attribute readable", true, attributes[0].isReadable());
      assertEquals("attribute writable", true, attributes[0].isWritable());
      assertEquals("attribute isIs", false, attributes[0].isIs());
   }

   public void testOperationInfo()
   {
      MBeanInfo info = getTrivialInfo();

      MBeanOperationInfo[] operations = info.getOperations();
      assertEquals("operations list length", 1, operations.length);
      assertEquals("operation name", "doOperation", operations[0].getName());
      assertEquals("operation return type", Void.TYPE.getName(), operations[0].getReturnType());
      assertEquals("operation impact", MBeanOperationInfo.UNKNOWN, operations[0].getImpact());

      MBeanParameterInfo[] params = operations[0].getSignature();
      assertEquals("signature length", 1, params.length);
      assertEquals("parameter type", String.class.getName(), params[0].getType());
   }

   public void testNotificationInfo()
   {
      MBeanInfo info = getTrivialInfo();

      MBeanNotificationInfo[] notifications = info.getNotifications();
      assertEquals("notification list length", 0, notifications.length);
   }


   private MBeanInfo getTrivialInfo()
   {
      MBeanInfo info = null;

      try
      {
         MBeanServer server = MBeanServerFactory.newMBeanServer();
         Trivial trivial = new Trivial();

         ObjectName name = new ObjectName("trivial:key=val");
         ObjectInstance instance = server.registerMBean(trivial, name);
         info = server.getMBeanInfo(name);
      }
      catch (MalformedObjectNameException e)
      {
         fail("got spurious MalformedObjectNameException");
      }
      catch (InstanceAlreadyExistsException e)
      {
         fail("got spurious InstanceAlreadyExistsException");
      }
      catch (MBeanRegistrationException e)
      {
         fail("got spurious MBeanRegistrationException");
      }
      catch (NotCompliantMBeanException e)
      {
         fail("got spurious NotCompliantMBeanException");
      }
      catch (InstanceNotFoundException e)
      {
         fail("got spurious InstanceNotFoundException");
      }
      catch (IntrospectionException e)
      {
         fail("got spurious IntrospectionException");
      }
      catch (ReflectionException e)
      {
         fail("got spurious ReflectionException");
      }

      return info;
   }
}
