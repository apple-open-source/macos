/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security.plugins;

import java.io.IOException;
import java.io.File;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.security.KeyStore;
import java.security.Security;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.security.auth.callback.CallbackHandler;

// JSSE key and trust managers
import com.sun.net.ssl.KeyManagerFactory;
import com.sun.net.ssl.TrustManagerFactory;

import org.jboss.mx.util.MBeanServerLocator;
import org.jboss.security.SecurityDomain;
import org.jboss.security.auth.callback.SecurityAssociationHandler;

/** The JaasSecurityDomain is an extension of JaasSecurityManager that addes
 the notion of a KeyStore, and JSSE KeyManagerFactory and TrustManagerFactory
 for supporting SSL and other cryptographic use cases.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.6.2.4 $
*/
public class JaasSecurityDomain
   extends JaasSecurityManager
   implements SecurityDomain, JaasSecurityDomainMBean
{
   private static boolean addedSunJSSEProvider;

   private int state;
   /** The KeyStore associated with the security domain.
    */
   private KeyStore keyStore;
   private KeyManagerFactory keyMgr;
   /** The KeyStore implementation type which defaults to 'JKS'.
    */
   private String keyStoreType = "JKS";
   private URL keyStoreURL;
   private char[] keyStorePassword;
   /** The JMX object name of the security manager service */
   private ObjectName managerServiceName = JaasSecurityManagerServiceMBean.OBJECT_NAME;
   private boolean loadSunJSSEProvider = true;

   /** Creates a default JaasSecurityDomain for with a securityDomain
    name of 'other'.
    */
   public JaasSecurityDomain()
   {
      super();
   }
   /** Creates a JaasSecurityDomain for with a securityDomain
    name of that given by the 'securityDomain' argument.
    @param securityDomain , the name of the security domain
    */
   public JaasSecurityDomain(String securityDomain)
   {
      this(securityDomain, new SecurityAssociationHandler());
   }
   /** Creates a JaasSecurityDomain for with a securityDomain
    name of that given by the 'securityDomain' argument.
    @param securityDomain , the name of the security domain
    @param handler , the CallbackHandler to use to obtain login module info
    */
   public JaasSecurityDomain(String securityDomain, CallbackHandler handler)
   {
      super(securityDomain, handler);
   }

   public KeyStore getKeyStore() throws SecurityException
   {
      return keyStore;
   }
   public KeyManagerFactory getKeyManagerFactory() throws SecurityException
   {
      return keyMgr;
   }

   public KeyStore getTrustStore() throws SecurityException
   {
      return null;
   }
   public TrustManagerFactory getTrustManagerFactory() throws SecurityException
   {
      return null;
   }
   /** The JMX object name string of the security manager service.
    @return The JMX object name string of the security manager service.
    */
   public ObjectName getManagerServiceName()
   {
      return this.managerServiceName;
   }
   /** Set the JMX object name string of the security manager service.
    */
   public void setManagerServiceName(ObjectName managerServiceName)
   {
      this.managerServiceName = managerServiceName;
   }

   public String getKeyStoreType()
   {
      return this.keyStoreType;
   }
   public void setKeyStoreType(String type)
   {
      this.keyStoreType = type;
   }
   public String getKeyStoreURL()
   {
      String url = null;
      if( keyStoreURL != null )
         url = keyStoreURL.toExternalForm();
      return url;
   }
   public void setKeyStoreURL(String storeURL) throws IOException
   {
      keyStoreURL = null;
      // First see if this is a URL
      try
      {
         keyStoreURL = new URL(storeURL);
      }
      catch(MalformedURLException e)
      {
         // Not a URL or a protocol without a handler
      }

      // Next try to locate this as file path
      if( keyStoreURL == null )
      {
         File tst = new File(storeURL);
         if( tst.exists() == true )
            keyStoreURL = tst.toURL();
      }

      // Last try to locate this as a classpath resource
      if( keyStoreURL == null )
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         keyStoreURL = loader.getResource(storeURL);
      }

      // Fail if no valid key store was located
      if( keyStoreURL == null )
      {
         String msg = "Failed to find url="+storeURL+" as a URL, file or resource";
         throw new MalformedURLException(msg);
      }
      log.debug("Using KeyStore="+keyStoreURL.toExternalForm());
   }
   public void setKeyStorePass(String password)
   {
      this.keyStorePassword = password.toCharArray();
   }   

   public boolean getLoadSunJSSEProvider()
   {
      return loadSunJSSEProvider;
   }
   public void setLoadSunJSSEProvider(boolean flag)
   {
      this.loadSunJSSEProvider = flag;
   }

   public String getName()
   {
      return "JaasSecurityDomain("+getSecurityDomain()+")";
   }   
   public int getState()
   {
      return state;
   }
   public String getStateString()
   {
      return states[state];
   }
   public void create()
   {
      log.info("Created");
   }
   public void start() throws Exception
   {
      if (getState() != STOPPED && getState() != FAILED)
         return;

      state = STARTING;
      log.info("Starting");

      // Install the Sun JSSE provider unless
      synchronized( JaasSecurityDomain.class )
      {
         if( loadSunJSSEProvider == true && addedSunJSSEProvider == false )
         {
            log.debug("Adding com.sun.net.ssl.internal.ssl.Provider");
            try
            {
               Security.addProvider(new com.sun.net.ssl.internal.ssl.Provider());
               addedSunJSSEProvider = true;
            }
            catch(Exception e)
            {
               log.warn("Failed to addProvider com.sun.net.ssl.internal.ssl.Provider", e);
            }
         }
      }

      if( keyStoreURL != null )
      {
         keyStore = KeyStore.getInstance(keyStoreType);
         InputStream is = keyStoreURL.openStream();
         keyStore.load(is, keyStorePassword);
         String algorithm = KeyManagerFactory.getDefaultAlgorithm();
         keyMgr = KeyManagerFactory.getInstance(algorithm);
         keyMgr.init(keyStore, keyStorePassword);
      }
      /* Register with the JaasSecurityManagerServiceMBean. This allows this
       JaasSecurityDomain to function as the security manager for security-domain
       elements that declare java:/jaas/xxx for our security domain name.
       */
      MBeanServer server = MBeanServerLocator.locateJBoss();
      Object[] params = {getSecurityDomain(), this};
      String[] signature = new String[] {"java.lang.String", "org.jboss.security.SecurityDomain"};
      server.invoke(managerServiceName, "registerSecurityDomain", params, signature);

      state = STARTED;
      log.info("Started");
   }
   public void stop()
   {
      // Uninstall the Sun JSSE provider unless
      synchronized( JaasSecurityDomain.class )
      {
         if( loadSunJSSEProvider == true && addedSunJSSEProvider == true )
         {
            log.debug("Removing com.sun.net.ssl.internal.ssl.Provider");
            try
            {
               String name = (new com.sun.net.ssl.internal.ssl.Provider()).getName();
               Security.removeProvider(name);
               addedSunJSSEProvider = false;
            }
            catch(Exception e)
            {
               log.warn("Failed to removeProvider com.sun.net.ssl.internal.ssl.Provider", e);
            }
         }
      }
      state = STOPPED;
   }
   public void destroy()
   {
   }
   
}

