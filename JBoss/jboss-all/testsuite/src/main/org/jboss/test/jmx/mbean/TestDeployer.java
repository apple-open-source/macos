/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.mbean;

import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;

import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.RuntimeMBeanException;
import javax.management.loading.MLet;

import java.io.File;

import org.jboss.system.Service;
import org.jboss.system.ServiceMBeanSupport;

import org.jboss.system.server.ServerConfigLocator;

/**
 * This is a do-nothing mbean to test service archive deployment.
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version <tt>$Revision: 1.6.4.1 $</tt>
 *
 * <b>20010901 david jencks</b>
 * <ul>
 *    <li>initial import
 * </ul>
 */
public class TestDeployer
       extends ServiceMBeanSupport
       implements TestDeployerMBean
{
   public String getBaseDir() throws Exception
   {
      File file = ServerConfigLocator.locate().getServerHomeDir();

      return file.getCanonicalPath();
   }

   protected void startService() throws Exception
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      log.info("TCL: "+loader);
      log.info("CodeSource: "+getClass().getProtectionDomain().getCodeSource());
   }
}
