/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

import java.lang.reflect.Method;
import javax.ejb.EJBContext;

/** An interface describing the requirements for a SecurityInterceptor proxy.
A SecurityProxy allows for the externalization of custom security checks 
on a per-method basis for both the EJB home and remote interface methods.
Custom security checks are those that cannot be described using the
standard EJB deployment time declarative role based security.

@author Scott.Stark@jboss.org
@version $Revision: 1.4.4.2 $
*/
public interface SecurityProxy
{
   /** Inform a proxy of the context in which it is operating.
    * @param beanHome The EJB remote home interface class
    * @param beanRemote The EJB remote interface class
    * @param securityMgr The security manager from the security domain
    * @throws InstantiationException
    */
   public void init(Class beanHome, Class beanRemote, Object securityMgr)
      throws InstantiationException;
   /** Inform a proxy of the context in which it is operating.
    * @param beanHome The EJB remote home interface class
    * @param beanRemote The EJB remote interface class
    * @param beanLocalHome The EJB local home interface class, may be null
    * @param beanLocal The EJB local interface class, may be null
    * @param securityMgr The security manager from the security domain
    * @throws InstantiationException
    */
   public void init(Class beanHome, Class beanRemote,
      Class beanLocalHome, Class beanLocal, Object securityMgr)
      throws InstantiationException;
    /** Called prior to any method invocation to set the current EJB context.
    */
    public void setEJBContext(EJBContext ctx);
    /** Called to allow the security proxy to perform any custom security
        checks required for the EJB remote or local home interface method.
    @param m , the EJB home or local home interface method
    @param args , the invocation args
    */
    public void invokeHome(Method m, Object[] args) throws Exception;
    /** Called to allow the security proxy to perform any custom security
        checks required for the EJB remote or local interface method.
    @param m , the EJB remote or local interface method
    @param args , the invocation args
    @param bean, the EJB implementation class instance
    */
    public void invoke(Method m, Object[] args, Object bean) throws Exception;
}
