/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.srp;

import java.io.Serializable;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import org.jboss.security.srp.SRPServerInterface;

/** A serializable proxy that is bound into JNDI with a reference to the
 RMI implementation of a SRPServerInterface. This allows a client to lookup
 the interface and not have the RMI stub for the server as it will be downloaded
 to them when the SRPServerProxy is unserialized.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.3.4.1 $
 */
public class SRPServerProxy implements InvocationHandler, Serializable
{
   /** The serial version UID @since 1.3 */
   private static final long serialVersionUID = 5255628656806648070L;

   private SRPServerInterface server;

   /** Create a SRPServerProxy given the SRPServerInterface that method
    invocations are to be delegated to.
    */
   SRPServerProxy(SRPServerInterface server)
   {
      this.server = server;
   }

   /** The InvocationHandler invoke method. All calls are simply delegated to
    the SRPServerInterface server object.
    */
   public Object invoke(Object proxy, Method method, Object[] args) throws Throwable
   {
      Object ret = null;
      try
      {
         ret = method.invoke(server, args);
      }
      catch (InvocationTargetException e)
      {
         throw e.getTargetException();
      }
      catch (Throwable e)
      {
         e.printStackTrace();
         throw e;
      }
      return ret;
   }
}
