/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.security.interceptors;

import java.security.Principal;
import java.util.Arrays;
import javax.naming.InitialContext;
import javax.management.MBeanInfo;

import org.jboss.mx.interceptor.AbstractInterceptor;
import org.jboss.mx.interceptor.Invocation;
import org.jboss.mx.interceptor.InvocationException;
import org.jboss.mx.server.MBeanInvoker;
import org.jboss.logging.Logger;
import org.jboss.security.srp.SRPSessionKey;
import org.jboss.security.srp.SRPServerSession;
import org.jboss.security.srp.jaas.SRPPrincipal;
import org.jboss.util.CachePolicy;

/** An interceptor that validates that the calling context has a valid SRP session
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class SRPCacheInterceptor
   extends AbstractInterceptor
{
   private static Logger log = Logger.getLogger(SRPCacheInterceptor.class);
   private String cacheJndiName;

   public SRPCacheInterceptor(MBeanInfo info, MBeanInvoker invoker)
   {
      super(info, invoker);
   }

   public void setAuthenticationCacheJndiName(String cacheJndiName)
   {
      this.cacheJndiName = cacheJndiName;
   }

   // Interceptor overrides -----------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      String opName = invocation.getName();
      log.info("invoke, opName=" + opName);
      if( opName.equals("testSession") == false )
      {
         Object value = getNext().invoke(invocation);
         return value;         
      }

      Object[] args = invocation.getArgs();
      Principal userPrincipal = (Principal) args[0];
      String username = userPrincipal.getName();
      byte[] clientChallenge = (byte[]) args[1];

      try
      {
         InitialContext iniCtx = new InitialContext();
         CachePolicy cache = (CachePolicy) iniCtx.lookup(cacheJndiName);
         SRPSessionKey key;
         if (userPrincipal instanceof SRPPrincipal)
         {
            SRPPrincipal srpPrincpal = (SRPPrincipal) userPrincipal;
            key = new SRPSessionKey(username, srpPrincpal.getSessionID());
         }
         else
         {
            key = new SRPSessionKey(username);
         }
         Object cacheCredential = cache.get(key);
         if (cacheCredential == null)
         {
            throw new SecurityException("No SRP session found for: " + key);
         }
         log.debug("Found SRP cache credential: " + cacheCredential);
         /** The cache object should be the SRPServerSession object used in the
          authentication of the client.
          */
         if (cacheCredential instanceof SRPServerSession)
         {
            SRPServerSession session = (SRPServerSession) cacheCredential;
            byte[] challenge = session.getClientResponse();
            boolean isValid = Arrays.equals(challenge, clientChallenge);
            if ( isValid == false )
               throw new SecurityException("Failed to validate SRP session key for: " + key);
         }
         else
         {
            throw new SecurityException("Unknown type of cache credential: " + cacheCredential.getClass());
         }
         log.debug("Validated SRP cache credential for: "+key);
      }
      catch (Exception e)
      {
         log.error("Invocation failed", e);
         throw new InvocationException(e, "Error validating caller");
      }

      Object value = getNext().invoke(invocation);
      return value;
   }
}
