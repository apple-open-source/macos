package org.jboss.test.security.service;

import org.jboss.system.ServiceMBean;

/** The JMX mbean interface for the SRP password verifier store.

@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
*/
public interface PropertiesVerifierStoreMBean extends ServiceMBean
{   /** Get the jndi name for the SRPVerifierSource implementation binding.
   */
   public String getJndiName();
   /** set the jndi name for the SRPVerifierSource implementation binding.
   */
   public void setJndiName(String jndiName);
}
