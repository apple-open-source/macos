/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

import java.lang.SecurityException;
import java.security.KeyStore;
// JSSE key and trust managers
import com.sun.net.ssl.KeyManagerFactory;
import com.sun.net.ssl.TrustManagerFactory;

/** The SecurityDomain interface combines the SubjectSecurityManager and
 RealmMapping interfaces and adds a keyStore and trustStore as well as
 JSSE KeyManagerFactory and TrustManagerFactory accessors for use with SSL/JSSE.

@see java.security.KeyStore
@see com.sun.net.ssl.KeyManagerFactory
@see com.sun.net.ssl.TrustManagerFactory

 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public interface SecurityDomain extends SubjectSecurityManager, RealmMapping
{

   /** Get the keystore associated with the security domain */
   public KeyStore getKeyStore() throws SecurityException;
   /** Get the KeyManagerFactory associated with the security domain */
   public KeyManagerFactory getKeyManagerFactory() throws SecurityException;

   /** Get the truststore associated with the security domain. This may be
    the same as the keystore. */
   public KeyStore getTrustStore() throws SecurityException;
   /** Get the TrustManagerFactory associated with the security domain */
   public TrustManagerFactory getTrustManagerFactory() throws SecurityException;
   
}
