/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.plugins;

import javax.management.ObjectName;

import org.jboss.mx.util.ObjectNameFactory;

/**
 * The JMX mbean interface for the SecurityPolicyService prototype.
 *
 * @author Scott.Stark@jboss.org
 *@version $Revision: 1.6.4.1 $
 */
public interface SecurityPolicyServiceMBean
   extends org.jboss.system.ServiceMBean
{
   ObjectName OBJECT_NAME = ObjectNameFactory.create(":service=SecurityPolicyService");

   /**
    * Get the jndi name under which the SRPServerInterface proxy should be bound
    */
   String getJndiName();
   
   /**
    * Set the jndi name under which the SRPServerInterface proxy should be bound
    */
   void setJndiName(String jndiName);
   
   String getPolicyFile();
   
   void setPolicyFile(String policyFile);
}
