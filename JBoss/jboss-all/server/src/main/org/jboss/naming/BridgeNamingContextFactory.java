/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.naming;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.lang.reflect.InvocationTargetException;
import java.util.Hashtable;
import javax.naming.Context;
import javax.naming.NameNotFoundException;
import javax.naming.NamingException;

import org.jnp.interfaces.NamingContextFactory;

/** A naming provider InitialContextFactory implementation that combines
 two Naming services to allow for delegation of lookups from one to the
 other. The default naming service is specified via the standard
 Context.PROVIDER_URL property while the secondary service is specified
 using an org.jboss.naming.provider.url2 property. An example of where
 this would be used is to bridge between the local JNDI service and the
 HAJNDI service. Lookups into the local JNDI service that fail would then
 try the HAJNDI service.

 @see javax.naming.spi.InitialContextFactory
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.1 $
 */
public class BridgeNamingContextFactory extends NamingContextFactory
{
   // InitialContextFactory implementation --------------------------
   public Context getInitialContext(Hashtable env)
      throws NamingException
   {
      Context primaryCtx = super.getInitialContext(env);
      Context bridgeCtx = primaryCtx;
      Object providerURL2 = env.get("org.jboss.naming.provider.url2");
      if( providerURL2 != null )
      {
         // A second provider url was given, create a secondary naming context
         Hashtable env2 = (Hashtable) env.clone();
         env2.put(Context.PROVIDER_URL, providerURL2);
         Context secondaryCtx = super.getInitialContext(env2);
         InvocationHandler h = new BridgeContext(primaryCtx, secondaryCtx);
         Class[] interfaces = {Context.class};
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         bridgeCtx = (Context) Proxy.newProxyInstance(loader, interfaces, h);
      }
      return bridgeCtx;
   }

   /** This class is the Context interface handler and performs the
       failed lookup delegation from the primary to secondary naming
       Context.
   */
   static class BridgeContext implements InvocationHandler
   {
      private Context primaryCtx;
      private Context secondaryCtx;

      BridgeContext(Context primaryCtx, Context secondaryCtx)
      {
         this.primaryCtx = primaryCtx;
         this.secondaryCtx = secondaryCtx;
      }

      public Object invoke(Object proxy, Method method, Object[] args)
            throws Throwable
      {
         Object value = null;
         // First try the primary context
         try
         {
            value = method.invoke(primaryCtx, args);
         }
         catch(InvocationTargetException e)
         {
            Throwable t = e.getTargetException();
            // Try the secondary if this is a failed lookup
            if( t instanceof NameNotFoundException && method.getName().equals("lookup") )
            {
               try
               {
                  value = method.invoke(secondaryCtx, args);
               }
               catch (InvocationTargetException e1)
               {
                  throw e1.getTargetException();
               }
            }
            else
            {
               throw t;
            }
         }
         return value;
      }
   }
}
