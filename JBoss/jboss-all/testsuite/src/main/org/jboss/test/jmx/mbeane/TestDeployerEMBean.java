/*
 * JBoss, the OpenSource J2EE server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jmx.mbeane;

import javax.management.ObjectName;

import org.w3c.dom.Element;
import org.w3c.dom.Document;

import org.jboss.mx.util.ObjectNameFactory;

import org.jboss.system.Service;
import org.jboss.system.ServiceMBean;

/** 
 * This is a little class to test deploying jsrs
 *   
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.3.4.2 $
 */
public interface TestDeployerEMBean
   extends Service, ServiceMBean
{
   public void accessUtilClass() throws Exception;
}
