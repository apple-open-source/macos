/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.invocation.http.interfaces;

import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.ObjectStreamException;
import java.io.OutputStream;
import java.io.ObjectOutputStream;
import java.net.Authenticator;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.lang.reflect.InvocationTargetException;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationException;
import org.jboss.invocation.MarshalledValue;
import org.jboss.logging.Logger;
import org.jboss.security.SecurityAssociationAuthenticator;

/** Common client utility methods
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.5 $
*/
public class Util
{
   /** A serialized MarshalledInvocation */
   private static String REQUEST_CONTENT_TYPE =
      "application/x-java-serialized-object; class=org.jboss.invocation.MarshalledInvocation";
   private static Logger log = Logger.getLogger(Util.class);
   /** The type of the HTTPS connection class */
   private static Class httpsConnClass;

   static
   {
      // Install the java.net.Authenticator to use
      try
      {
         Authenticator.setDefault(new SecurityAssociationAuthenticator());
      }
      catch(Exception e)
      {
         log.warn("Failed to install SecurityAssociationAuthenticator", e);
      }
      // Determine the type of the HttpsURLConnection in this runtime
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      try
      {
         // First look for the JDK 1.4 JSSE Https connection
         httpsConnClass = loader.loadClass("javax.net.ssl.HttpsURLConnection");
         log.debug("httpsConnClass: "+httpsConnClass);
      }
      catch(Exception e)
      {
         // Next try the JSSE external dist Https connection
         try
         {
            httpsConnClass = loader.loadClass("com.sun.net.ssl.HttpsURLConnection");
            log.debug("httpsConnClass: "+httpsConnClass);
         }
         catch(Exception e2)
         {
            log.warn("No HttpsURLConnection seen");
         }
      }
   }

   /** Install the SecurityAssociationAuthenticator as the default
    * java.net.Authenticator
    */
   public static void init()
   {
      Authenticator.setDefault(new SecurityAssociationAuthenticator());
   }

   /** Post the Invocation as a serialized MarshalledInvocation object. This is
    using the URL class for now but this should be improved to a cluster aware
    layer with full usage of HTTP 1.1 features, pooling, etc.
   */
   public static Object invoke(URL externalURL, Invocation mi)
      throws Exception
   {
      if( log.isTraceEnabled() )
         log.trace("invoke, externalURL="+externalURL);
      /* Post the MarshalledInvocation data. This is using the URL class
       for now but this should be improved to a cluster aware layer with
       full usage of HTTP 1.1 features, pooling, etc.
       */
      HttpURLConnection conn = (HttpURLConnection) externalURL.openConnection();
      configureHttpsHostVerifier(conn);
      conn.setDoInput(true);
      conn.setDoOutput(true);
      conn.setRequestProperty("ContentType", REQUEST_CONTENT_TYPE);
      conn.setRequestMethod("POST");
      OutputStream os = conn.getOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(os);
      try
      {
         oos.writeObject(mi);
         oos.flush();
      }
      catch (ObjectStreamException e)
      {
         // This generally represents a programming/deployment error,
         // not a communication problem
         throw new InvocationException(e);
      }

      // Get the response MarshalledValue object
      InputStream is = conn.getInputStream();
      ObjectInputStream ois = new ObjectInputStream(is);
      MarshalledValue mv = (MarshalledValue) ois.readObject();
      ois.close();
      oos.close();

      // If the encoded value is an exception throw it
      Object value = mv.get();
      if( value instanceof Exception )
      {
         throw (Exception) value;
      }

      return value;
   }

   /** Given an Https URL connection check the org.jboss.security.ignoreHttpsHost
    * system property and if true, install the AnyhostVerifier as the 
    * com.sun.net.ssl.HostnameVerifier or javax.net.ssl.HostnameVerifier
    * depending on the version of JSSE seen. If HttpURLConnection is not a
    * HttpsURLConnection then nothing is done.
    *  
    * @param conn a HttpsURLConnection
    * @throws InvocationTargetException on failure to set the 
    * @throws IllegalAccessException
    */ 
   public static void configureHttpsHostVerifier(HttpURLConnection conn)
      throws InvocationTargetException, IllegalAccessException
   {
      boolean isAssignable = httpsConnClass.isAssignableFrom(conn.getClass());
      if( isAssignable )
      {
         // See if the org.jboss.security.ignoreHttpsHost property is set
         if( Boolean.getBoolean("org.jboss.security.ignoreHttpsHost") == true )
         {
            AnyhostVerifier.setHostnameVerifier(conn);
         }
      }
   }

   /** First try to use the externalURLValue as a URL string and if this
       fails to produce a valid URL treat the externalURLValue as a system
       property name from which to obtain the URL string. This allows the
       proxy url to not be set until the proxy is unmarshalled in the client
       vm, and is necessary when the server is sitting behind a firewall or
       proxy and does not know what its public http interface is named.
   */
   public static URL resolveURL(String urlValue) throws MalformedURLException
   {
      if( urlValue == null )
         return null;

      URL externalURL = null;
      try
      {
         externalURL = new URL(urlValue);
      }
      catch(MalformedURLException e)
      {
         // See if externalURL refers to a property
         String urlProperty = System.getProperty(urlValue);
         if( urlProperty == null )
            throw e;
         externalURL = new URL(urlProperty);
      }
      return externalURL;
   }
}
