/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.naming;

import java.io.InputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Hashtable;
import javax.naming.Context;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.Reference;
import javax.naming.RefAddr;
import javax.naming.spi.InitialContextFactory;
import javax.naming.spi.ObjectFactory;

import org.jboss.invocation.MarshalledValue;
import org.jboss.invocation.http.interfaces.AnyhostVerifier;
import org.jboss.invocation.http.interfaces.Util;
import org.jboss.logging.Logger;
import org.jnp.interfaces.Naming;
import org.jnp.interfaces.NamingContext;

/** A naming provider InitialContextFactory implementation that obtains a
 Naming proxy from an HTTP URL.

 @see javax.naming.spi.InitialContextFactory

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.2.3 $
 */
public class HttpNamingContextFactory
   implements InitialContextFactory, ObjectFactory
{
   private static Logger log = Logger.getLogger(HttpNamingContextFactory.class);

   // InitialContextFactory implementation --------------------------
   public Context getInitialContext(Hashtable env)
      throws NamingException
   {
      // Parse the Context.PROVIDER_URL
      String provider = (String) env.get(Context.PROVIDER_URL);
      if( provider.startsWith("jnp:") == true )
         provider = "http:" + provider.substring(4);
      else if( provider.startsWith("jnps:") == true )
         provider = "https:" + provider.substring(5);
      else if( provider.startsWith("jnp-http:") == true )
         provider = "http:" + provider.substring(9);
      else if( provider.startsWith("jnp-https:") == true )
         provider = "https:" + provider.substring(10);

      URL providerURL = null;
      Naming namingServer = null;
      try
      {
         providerURL = new URL(provider);
         // Retrieve the Naming interface
         namingServer = getNamingServer(providerURL);
      }
      catch(Exception e)
      {
         NamingException ex = new NamingException("Failed to retrieve Naming interface");
         ex.setRootCause(e);
         throw ex;
      }

      // Copy the context env
      env = (Hashtable) env.clone();
      return new NamingContext(env, null, namingServer);
   }

   // ObjectFactory implementation ----------------------------------
   public Object getObjectInstance(Object obj, Name name, Context nameCtx,
      Hashtable env)
      throws Exception
   {
      Context ctx = getInitialContext(env);
      Reference ref = (Reference) obj;
      RefAddr addr = ref.get("URL");
      String path = (String) addr.getContent();
      return ctx.lookup(path);
   }

   private Naming getNamingServer(URL providerURL) throws ClassNotFoundException, IOException
   {
      // Initialize the proxy Util class to integrate JAAS authentication
      Util.init();
      log.debug("Retrieving content from : "+providerURL);
      HttpURLConnection conn = (HttpURLConnection) providerURL.openConnection();
      if( conn instanceof com.sun.net.ssl.HttpsURLConnection )
      {
         // See if the org.jboss.security.ignoreHttpsHost property is set
         if( Boolean.getBoolean("org.jboss.security.ignoreHttpsHost") == true )
         {
            com.sun.net.ssl.HttpsURLConnection sconn = (com.sun.net.ssl.HttpsURLConnection) conn;
            AnyhostVerifier verifier = new AnyhostVerifier();
            sconn.setHostnameVerifier(verifier);
         }
      }
      int length = conn.getContentLength();
      String type = conn.getContentType();
      log.debug("ContentLength: "+length+"\nContentType: "+type);

      InputStream is = conn.getInputStream();
      ObjectInputStream ois = new ObjectInputStream(is);
      MarshalledValue mv = (MarshalledValue) ois.readObject();
      ois.close();

      Naming namingServer = (Naming) mv.get();
      return namingServer;
   }
}
