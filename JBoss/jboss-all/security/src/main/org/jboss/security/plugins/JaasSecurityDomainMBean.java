/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.plugins;

import java.io.IOException;
import javax.management.ObjectName;
import org.jboss.system.ServiceMBean;


/** The JaasSecurityDomainMBean adds support for KeyStore management.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.4 $
*/
public interface JaasSecurityDomainMBean extends ServiceMBean
{
   /** KeyStore implementation type being used.
    @return the KeyStore implementation type being used.
    */
   public String getKeyStoreType();
   /** Set the type of KeyStore implementation to use. This is
    passed to the KeyStore.getInstance() factory method.
    */
   public void setKeyStoreType(String type);
   /** Get the KeyStore database URL string.
    */
   public String getKeyStoreURL();
   /** Set the KeyStore database URL string. This is used to obtain
    an InputStream to initialize the KeyStore.
    */
   public void setKeyStoreURL(String storeURL) throws IOException;
    /** Set the credential string for the KeyStore.
    */
   public void setKeyStorePass(String password);
   /** The JMX object name string of the security manager service.
    @return The JMX object name string of the security manager service.
    */
   public ObjectName getManagerServiceName();
   /** Set the JMX object name string of the security manager service.
    */
   public void setManagerServiceName(ObjectName jmxName);
   /** A flag indicating if the Sun com.sun.net.ssl.internal.ssl.Provider 
    security provider should be loaded on startup. This is needed when using
    the Sun JSSE jars without them installed as an extension with JDK 1.3. This
    should be set to false with JDK 1.4 or when using an alternate JSSE provider
    */
   public boolean getLoadSunJSSEProvider();

   /** A flag indicating if the Sun com.sun.net.ssl.internal.ssl.Provider 
    security provider should be loaded on startup. This is needed when using
    the Sun JSSE jars without them installed as an extension with JDK 1.3. This
    should be set to false with JDK 1.4 or when using an alternate JSSE provider
    */
   public void setLoadSunJSSEProvider(boolean flag);
}
