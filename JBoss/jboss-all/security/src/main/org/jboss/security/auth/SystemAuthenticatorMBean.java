/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.security.auth;

import org.jboss.system.Service;

/** An MBean that requires a JAAS login in order for it to startup. This 
 * cam be used to require a login to startup the JBoss server.
 *
 * @version $Revision: 1.1.2.1 $
 * @author Scott.Stark@jboss.org
 */
public interface SystemAuthenticatorMBean extends Service
{
   /** Get the name of the security domain used for authentication
    */
   public String getSecurityDomain();
   /** Set the name of the security domain used for authentication
    */
   public void setSecurityDomain(String name);

   /** Get the CallbackHandler to use to obtain the authentication
    information.
    @see javax.security.auth.callback.CallbackHandler
    */
   public Class getCallbackHandler();
   /** Specify the CallbackHandler to use to obtain the authentication
    information.
    @see javax.security.auth.callback.CallbackHandler
    */
   public void setCallbackHandler(Class callbackHandlerClass)
      throws InstantiationException, IllegalAccessException;
}
