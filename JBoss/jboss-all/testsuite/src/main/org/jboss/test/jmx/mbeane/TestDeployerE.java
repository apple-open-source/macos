/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.mbeane;

import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;

import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.RuntimeMBeanException;
import javax.management.loading.MLet;


import org.jboss.system.Service;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.test.jmx.mbeane.util.UtilClass1;
import org.jboss.test.jmx.mbeane.util.UtilClass2;

/** This mbean services uses a class contained in a jar within the sar
 * deployment to test
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.1.4.1 $ <p>
 */
public class TestDeployerE
       extends ServiceMBeanSupport
       implements TestDeployerEMBean
{


   public void accessUtilClass() throws Exception
   {
      String info = UtilClass2.INFO;
      log.info("Using UtilClass2 info:" + info);
   }

   protected void startService() throws Exception
   {
      String info = UtilClass1.INFO;
      log.info("Using UtilClass1 info:" + info);
   }
}
