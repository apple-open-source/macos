/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jetty.http;

import java.io.IOException;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.net.ssl.SSLServerSocketFactory;

import org.jboss.security.SecurityDomain;
import org.jboss.security.ssl.DomainServerSocketFactory;

import org.mortbay.http.JsseListener;

/** A subclass of JsseListener that uses the KeyStore associated with the
 * SecurityDomain given by the SecurityDomain attribute.
 *
 * @see org.jboss.security.SecurityDoamin
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class SecurityDomainListener extends JsseListener
{
   private String securityDomainName;
   private SecurityDomain securityDomain;

   public SecurityDomainListener()
       throws IOException
   {
       super();
   }

   public String getSecurityDomain()
   {
      return securityDomainName;
   }
   public void setSecurityDomain(String securityDomainName)
      throws NamingException
   {
      this.securityDomainName = securityDomainName;
      InitialContext iniCtx = new InitialContext();
      this.securityDomain = (SecurityDomain) iniCtx.lookup(securityDomainName);
   }

   protected SSLServerSocketFactory createFactory() throws Exception
   {
      DomainServerSocketFactory dssf = new DomainServerSocketFactory(securityDomain);
      return dssf;
   }
   
}
