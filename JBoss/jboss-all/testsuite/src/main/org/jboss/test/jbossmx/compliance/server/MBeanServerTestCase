/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.server;

import org.jboss.test.jbossmx.compliance.TestCase;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.Attribute;
import javax.management.InstanceNotFoundException;
import javax.management.AttributeNotFoundException;
import javax.management.MBeanException;
import javax.management.RuntimeMBeanException;
import javax.management.RuntimeErrorException;
import javax.management.InvalidAttributeValueException;

import org.jboss.test.jbossmx.compliance.server.support.Test;
import org.jboss.test.jbossmx.compliance.server.support.TestMBean;
import org.jboss.test.jbossmx.compliance.server.support.MyScreamingException;
import org.jboss.test.jbossmx.compliance.server.support.ExceptionOnTheRun;
import org.jboss.test.jbossmx.compliance.server.support.BabarError;


public class MBeanServerTestCase
   extends TestCase 
{
   public MBeanServerTestCase(String s)
   {
      super(s);
   }

   public void testInvokeWithNonExistantMBean()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         server.invoke(new ObjectName(":mbean=doesnotexist"), "noMethod", null, null);
         
         // should not reach here
         fail("InstanceNotFoundException was not thrown from an invoke operation on a non-existant MBean.");
      }
      catch (InstanceNotFoundException e)
      {
         // should get here
      }
      catch (Throwable t) 
      {
         log.debug("failed", t);
         fail("Unexpected error on server.invoke(NonExistantMBean): " + t.toString());
      }
   }
   
   public void testInvokeWithBusinessException()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:test=test");
         server.registerMBean(new Test(), name);
         
         server.invoke(name, "operationWithException", null, null);
         
         // should not get here
         fail("MBeanException was not thrown.");
      }
      catch (MBeanException e)
      {
         // this is expected
         assertTrue(e.getTargetException() instanceof MyScreamingException);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   
   public void testGetAttributeWithNonExistingAttribute()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         Object foo = server.getAttribute(new ObjectName(MBEAN_SERVER_DELEGATE), "Foo");
         
         // should not reach here
         fail("AttributeNotFoundexception was not thrown when invoking getAttribute() call on a non-existant attribute.");
      }
      catch (AttributeNotFoundException e)
      {
         // Expecting this.
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   public void testGetAttributeWithBusinessException()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:test=test");
         server.registerMBean(new Test(), name);
         
         Object foo = server.getAttribute(name, "ThisWillScream");
         
         // should not reach here
         fail("Did not throw the screaming exception");
      }
      catch (MBeanException e)
      {
         // this is expected
         // FIXME THS - is this a valid test?
         //assertTrue(e.getMessage().startsWith("Exception thrown by attribute"));
         assertTrue(e.getTargetException() instanceof MyScreamingException);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   public void testGetAttributeWithNonExistingMBean()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:name=DoesNotExist");
         
         server.getAttribute(name, "Whatever");
         
         // should not reach here
         fail("InstanceNotFoundException was not thrown on a nonexistant MBean.");
      }
      catch (InstanceNotFoundException e)
      {
         // this is expected
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   public void testGetAttributeWithUncheckedException()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:test=test");
         server.registerMBean(new Test(), name);
         
         server.getAttribute(name, "ThrowUncheckedException");
         
         // should not reach here
         fail("RuntimeMBeanException was not thrown");
      }
      catch (RuntimeMBeanException e)
      {
         // this is expected
         assertTrue(e.getTargetException() instanceof ExceptionOnTheRun);
      }
      catch (Throwable t)
      {
         fail("Unexpected err0r: " + t.toString());
      }
   }
   
   public void testGetAttributeWithError()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:test=test");
         server.registerMBean(new Test(), name);
         
         server.getAttribute(name, "Error");
         
         // should not reach here
         fail("Error was not thrown");
      }
      catch (RuntimeErrorException e)
      {
         // this is expected
         assertTrue(e.getTargetError() instanceof BabarError);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   public void testSetAttributeWithNonExistingAttribute()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         server.setAttribute(new ObjectName(MBEAN_SERVER_DELEGATE), new Attribute("Foo", "value"));
         
         // should not reach here
         fail("AttributeNotFoundexception was not thrown when invoking getAttribute() call on a non-existant attribute.");
      }
      catch (AttributeNotFoundException e)
      {
         // Expecting this.
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   public void testSetAttributeWithBusinessException()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:test=test");
         server.registerMBean(new Test(), name);
         
         server.setAttribute(name, new Attribute("ThisWillScream", "value"));
         
         // should not reach here
         fail("Did not throw the screaming exception");
      }
      catch (MBeanException e)
      {
         // this is expected
         // FIXME THS - commented the assertion below: is that really what's required?
         // assertTrue(e.getMessage().startsWith("Exception thrown by attribute"));
         assertTrue(e.getTargetException() instanceof MyScreamingException);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   public void testSetAttributeWithNonExistingMBean()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:name=DoesNotExist");
         
         server.setAttribute(name, new Attribute("Whatever", "nothing"));
         
         // should not reach here
         fail("InstanceNotFoundException was not thrown on a nonexistant MBean.");
      }
      catch (InstanceNotFoundException e)
      {
         // this is expected
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }      
   }
   
   public void testSetAttributeWithUncheckedException()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:test=test");
         server.registerMBean(new Test(), name);
         
         server.setAttribute(name, new Attribute("ThrowUncheckedException", "value"));
         
         // should not reach here
         fail("RuntimeMBeanException was not thrown");
      }
      catch (RuntimeMBeanException e)
      {
         // this is expected
         assertTrue(e.getTargetException() instanceof ExceptionOnTheRun);
      }
      catch (Throwable t)
      {
         fail("Unexpected err0r: " + t.toString());
      }      
   }
   
   public void testSetAttributeWithError()
   {
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         ObjectName name = new ObjectName("test:test=test");
         server.registerMBean(new Test(), name);
         
         server.setAttribute(name, new Attribute("Error", "value"));
         
         // should not reach here
         fail("Error was not thrown");
      }
      catch (RuntimeErrorException e)
      {
         // this is expected
         assertTrue(e.getTargetError() instanceof BabarError);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }      
   }
   
}
