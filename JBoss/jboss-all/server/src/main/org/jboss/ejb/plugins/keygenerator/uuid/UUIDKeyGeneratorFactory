/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins.keygenerator.uuid;

import javax.management.JMException;
import javax.management.ObjectName;
import javax.naming.Context;
import javax.naming.InitialContext;

import org.jboss.system.ServiceMBean;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.naming.Util;

import org.jboss.ejb.plugins.keygenerator.KeyGeneratorFactory;
import org.jboss.ejb.plugins.keygenerator.KeyGenerator;

/**
 * MBean interface for UUIDKeyGeneratorFactoryService
 *
 * @author <a href="mailto:loubyansky@ukr.net">Alex Loubyansky</a>
 *
 * @version $Revision: 1.1.2.2 $
 */
public interface UUIDKeyGeneratorFactoryServiceMBean
   extends org.jboss.system.ServiceMBean
{
   //default object name
   public static final javax.management.ObjectName OBJECT_NAME =
      org.jboss.mx.util.ObjectNameFactory.create(
         "jboss.system:service=KeyGeneratorFactory,type=UUID");
}
