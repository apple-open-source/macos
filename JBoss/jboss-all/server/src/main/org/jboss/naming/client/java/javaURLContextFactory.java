/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.naming.client.java;

import java.util.Hashtable;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import javax.naming.Context;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.InitialContext;
import javax.naming.OperationNotSupportedException;
import javax.naming.NameParser;
import javax.naming.spi.ObjectFactory;

/** The external client java URL context factory. This is used in conjunction
 * with j2ee application clients to implement the java:comp/env
 * enterprise naming context (ENC).
 *     
 * @see javax.naming.spi.ObjectFactory
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class javaURLContextFactory
   implements ObjectFactory
{
   public static final String J2EE_CLIENT_NAME_PROP = "j2ee.clientName";


   // ObjectFactory implementation ----------------------------------
   public Object getObjectInstance(Object obj, Name name, Context nameCtx,
      Hashtable env)
      throws Exception
   {
      // Get the j2ee.clientName value
      String clientName = (String) env.get(J2EE_CLIENT_NAME_PROP);
      if( clientName == null )
         throw new NamingException("Failed to find j2ee.clientName in jndi env");

      Object result = null;

      if( nameCtx == null )
         nameCtx = new InitialContext(env);
      if( obj == null )
      {
         // Create a context for resolving the java: url
         InvocationHandler handler = new EncContextProxy(nameCtx, clientName);
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         Class[] ifaces = {Context.class};
         result = Proxy.newProxyInstance(loader, ifaces, handler);
      }
      return result;
   }

   private static class EncContextProxy implements InvocationHandler
   {
      Context lookupCtx;
      String clientName;
      EncContextProxy(Context lookupCtx, String clientName)
      {
         this.lookupCtx = lookupCtx;
         this.clientName = clientName;
      }

      /**
       */
      public Object invoke(Object proxy, Method method, Object[] args)
         throws Throwable
      {
         String methodName = method.getName();
         if( methodName.equals("toString") == true )
            return "Client ENC("+clientName+")";

         if( methodName.equals("lookup") == false )
            throw new OperationNotSupportedException("Only lookup is supported, op="+method);
         NameParser parser = lookupCtx.getNameParser("");
         Name name = null;
         if( args[0] instanceof String )
            name = parser.parse((String) args[0]);
         else
           name = (Name)args[0];

         // Lookup the client application context from the server
         Context clientCtx = (Context) lookupCtx.lookup(clientName);
         // Strip the comp/env prefix
         Name bindingName = name.getSuffix(2);
         Object binding = clientCtx.lookup(bindingName);
         return binding;
      }
   }
}
