/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp;

import java.io.IOException;

import org.jboss.system.ServiceMBean;

/** The JMX mbean interface for the SRP password verifier store.

@author Scott.Stark@jboss.org
@version $Revision: 1.3 $
*/
public interface SRPVerifierStoreServiceMBean extends ServiceMBean
{
   /** Get the jndi name for the SRPVerifierSource implementation binding.
    */
    public String getJndiName();
   /** set the jndi name for the SRPVerifierSource implementation binding.
    */
    public void setJndiName(String jndiName);
    /** Set the location of the user password verifier store
    */
    public void setStoreFile(String fileName) throws IOException;
    /** Add a user to the store.
    */
    public void addUser(String username, String password) throws IOException;
    /** Delete a user to the store.
    */
    public void delUser(String username) throws IOException;
}
