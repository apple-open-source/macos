/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jmx.test;

import javax.management.Attribute;
import javax.management.ObjectName;

import junit.framework.Test;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jmx.invoker.CustomClass;
import org.jboss.test.jmx.invoker.InvokerTestMBean;

/** Tests of mbean attributes.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class AttributesUnitTestCase
   extends JBossTestCase
{
   public AttributesUnitTestCase(String name)
   {
      super(name);
   }

   public static Test suite()
      throws Exception
   {
      return getDeploySetup(AttributesUnitTestCase.class, "attrtest.sar");
   }

   public void testXmlString()
      throws Exception
   {
      getLog().info("+++ testXmlString");
      RMIAdaptor server = super.getServer();
      ObjectName serviceName = new ObjectName("test:name=AttrTests,case=#1");
      String xml = (String) server.getAttribute(serviceName, "XmlString");
      getLog().info("XmlString: '"+xml+"'");
      String expectedXml = "<depinfo>\n<value name='abc'>A Value</value>\n</depinfo>";
      assertTrue("xml cdata as expected", xml.equals(expectedXml));
   }

   public void testSysPropRef()
      throws Exception
   {
      RMIAdaptor server = super.getServer();
      ObjectName serviceName = new ObjectName("test:name=AttrTests,case=#1");
      String prop = (String) server.getAttribute(serviceName, "SysPropRef");
      assertTrue("prop has been replaced", prop.equals("${java.vm.version}") == false);
   }

   public void testTrimedString()
      throws Exception
   {
      RMIAdaptor server = super.getServer();
      ObjectName serviceName = new ObjectName("test:name=AttrTests,case=#1");
      String prop = (String) server.getAttribute(serviceName, "TrimedString");
      assertTrue("whitespace is trimed", prop.equals("123456789"));
   }

   public void testSysPropRefNot()
      throws Exception
   {
      RMIAdaptor server = super.getServer();
      ObjectName serviceName = new ObjectName("test:name=AttrTests,case=#2");
      String prop = (String) server.getAttribute(serviceName, "SysPropRef");
      assertTrue("prop has NOT been replaced", prop.equals("${java.vm.version}"));
   }

   public void testTrimedStringNot()
      throws Exception
   {
      RMIAdaptor server = super.getServer();
      ObjectName serviceName = new ObjectName("test:name=AttrTests,case=#2");
      String prop = (String) server.getAttribute(serviceName, "TrimedString");
      assertTrue("whitespace is NOT trimed", prop.equals(" 123456789 "));
   }
}
