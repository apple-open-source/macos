/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.classloader.test;

import javax.management.Attribute;
import javax.management.ObjectName;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestServices;
import org.jboss.test.JBossTestSetup;

/**
 * Unit tests for the org.jboss.deployment.ClasspathExtension
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class ClasspathExtensionUnitTestCase extends JBossTestCase
{
   ObjectName extension;

   public ClasspathExtensionUnitTestCase(String name)
   {
      super(name);

      try
      {
         extension = new ObjectName("jboss.test:test=ClasspathTest,mbean=extension");
      }
      catch (Exception ignored)
      {
      }
   }

   public void testClasspathExtension() throws Exception
   {
      assertTrue("Shouldn't find the resource before deployment", findResource() == false);
      getServer().createMBean("org.jboss.deployment.ClasspathExtension", extension);
      try
      {
         getServer().setAttribute(extension, new Attribute("MetadataURL", getDeployURL("")));
         getServer().invoke(extension, "create", new Object[0], new String[0]);
         getServer().invoke(extension, "start", new Object[0], new String[0]);
         assertTrue("Should find the resource after deployment", findResource());
      }
      finally
      {
         getServer().invoke(extension, "stop", new Object[0], new String[0]);
         getServer().invoke(extension, "destroy", new Object[0], new String[0]);
         getServer().unregisterMBean(extension);
      }
      assertTrue("Shouldn't find the resource after undeployment", findResource() == false);
   }

   private boolean findResource()
      throws Exception
   {
      return ((Boolean) getServer().invoke(new ObjectName("jboss.test:test=ClasspathTest"), "findResource",
         new Object[] { "classpath.sar" }, new String[] { String.class.getName() })).booleanValue();
   }

   public static Test suite() throws Exception
   {
      return new JBossTestSetup(getDeploySetup(ClasspathExtensionUnitTestCase.class, "classpath.sar"));
   }
}
