/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.http;

// This interface is javax.net.ssl.HostnameVerifier in JDK1.4+
import com.sun.net.ssl.HostnameVerifier;

/* An implementation of the HostnameVerifier that accepts any SSL certificate
 * hostname as matching the https URL that was used to initiate the SSL connection.
 * This is useful for testing SSL setup in development environments using self
 * signed SSL certificates.
 *
 * This is a duplicate object found here and in the server module.  Like the
 * Base64Encoder, we'll probably want to move it somewhere else.  nathan@jboss.org
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.2.1 $
 */
public class AnyhostVerifier implements HostnameVerifier
{
    public boolean verify(String urlHostname, String certHostname)
    {
        return true;
    }   
}