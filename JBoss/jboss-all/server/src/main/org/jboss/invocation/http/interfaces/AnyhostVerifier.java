/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.http.interfaces;

// This interface is javax.net.ssl.HostnameVerifier in JDK1.4+
import com.sun.net.ssl.HostnameVerifier;

/* An implementation of the HostnameVerifier that accepts any SSL certificate
hostname as matching the https URL that was used to initiate the SSL connection.
This is useful for testing SSL setup in development environments using self
signed SSL certificates.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.1 $
 */
public class AnyhostVerifier implements HostnameVerifier
{
   public boolean verify(String urlHostname, String certHostname)
   {
      return true;
   }
}
