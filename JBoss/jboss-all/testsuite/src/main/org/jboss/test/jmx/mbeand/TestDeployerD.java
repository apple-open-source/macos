/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.mbeand;

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

/**
 *  This is a do-nothing mbean to test service archive deployment.
 *
 * @author     <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version    $Revision: 1.2 $ <p>
 *
 *      <b>20010901 david jencks</b>
 *      <ul>initial import
 *        <li>
 *      </ul>
 *
 */
public class TestDeployerD
       extends ServiceMBeanSupport
       implements TestDeployerDMBean
{
   public String getName()
   {
      return "TestDeployerD";
   }

}
