/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ha.httpsession.beanimpl.ejb;

import java.io.Serializable;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.PayloadKey;
import org.jboss.proxy.Interceptor;

/** An InvokerInterceptor that places the client thread context ClassLoader
 * into the invocation for use by the s ServerTCLInterceptor.
 *
 * @see org.jboss.ha.httpsession.beanimpl.ejb.ServerTCLInterceptor
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class ClientTCLInterceptor
      extends Interceptor
      implements Serializable
{
   /** The serialVersionUID
    * @since 1.1.2.1
    */ 
   private static final long serialVersionUID = 1092404482795617680L;

   public ClientTCLInterceptor()
   {
   }

   // Public --------------------------------------------------------

   /** Add the current thread context ClassLoader to the invocation as a
    * transient value under the key "org.jboss.invocation.TCL"
    */
   public Object invoke(Invocation invocation)
         throws Throwable
   {
      ClassLoader tcl = Thread.currentThread().getContextClassLoader();
      invocation.setValue("org.jboss.invocation.TCL", tcl, PayloadKey.TRANSIENT);
      return getNext().invoke(invocation);
   }
}
