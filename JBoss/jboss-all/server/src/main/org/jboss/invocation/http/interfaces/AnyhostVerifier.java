/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.http.interfaces;

import java.net.HttpURLConnection;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/* An implementation of the HostnameVerifier that accepts any SSL certificate
hostname as matching the https URL that was used to initiate the SSL connection.
This is useful for testing SSL setup in development environments using self
signed SSL certificates.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.2 $
 */
public class AnyhostVerifier implements InvocationHandler
{
   public static void setHostnameVerifier(HttpURLConnection conn)
      throws IllegalAccessException, InvocationTargetException
   {
      Class httpsConnClass = conn.getClass();
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      InvocationHandler handler = new AnyhostVerifier();
      // Get the HostnameVerifier type by calling getHostnameVerifier()
      Class hostnameVerifierClass = null;
      Method getHostnameVerifier = null;
      Method setHostnameVerifier = null;
      Class[] interfaces = {null};
      try
      {
         Class[] empty = {};
         getHostnameVerifier = httpsConnClass.getMethod("getHostnameVerifier", empty);
         hostnameVerifierClass = getHostnameVerifier.getReturnType();
         interfaces[0] = hostnameVerifierClass;
         setHostnameVerifier = httpsConnClass.getMethod("setHostnameVerifier", interfaces);
      }
      catch(NoSuchMethodException e)
      {
         throw new InvocationTargetException(e);
      }

      Object verifier = Proxy.newProxyInstance(loader, interfaces, handler);
      Object[] args = {verifier};
      setHostnameVerifier.invoke(conn, args);
   }

   /** An implementation of the com.sun.net.ssl.HostnameVerifier or 
    * javax.net.ssl.HostnameVerifier that returns true always.
    * 
    * @param proxy
    * @param method the HostnameVerifier method invocation
    * @param args the HostnameVerifier method args
    * @return Boolean.TRUE for anything but toString invocations
    * @throws Throwable
    */ 
   public Object invoke(Object proxy, Method method, Object[] args)
      throws Throwable
   {
      String name = method.getName();
      if( name.equals("toString"))
         return super.toString();
      return Boolean.TRUE;
   }
}
