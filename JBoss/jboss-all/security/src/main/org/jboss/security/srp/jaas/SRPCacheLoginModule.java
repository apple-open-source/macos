/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp.jaas;

import java.security.Principal;
import java.util.Arrays;
import java.util.Map;
import java.util.Set;
import javax.crypto.spec.SecretKeySpec;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.callback.Callback;
import javax.security.auth.callback.UnsupportedCallbackException;
import javax.security.auth.login.LoginException;
import javax.security.auth.spi.LoginModule;

import org.jboss.logging.Logger;
import org.jboss.security.auth.callback.SecurityAssociationCallback;
import org.jboss.security.srp.SRPParameters;
import org.jboss.security.srp.SRPServerSession;
import org.jboss.security.srp.SRPSessionKey;
import org.jboss.util.CachePolicy;

/** A server side login module that validates a username and
 session client challenge response against the cache of authentication
 info maintained by the SRPService mbean. This module needs
 a CallbackHandler that supplies the user principal and
 credential via the SecurityAssociationCallback object.
 
 module options:
 cacheJndiName, the JNDI name of the CachePolicy of <Principal,SRPServerSession>
 information managed by the SRPSerice.
 domainName,
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.7.4.2 $
 */
public class SRPCacheLoginModule implements LoginModule
{
   private static Logger log = Logger.getLogger(SRPCacheLoginModule.class);
   private Subject subject;
   private CallbackHandler handler;
   private Map sharedState;
   private String domainName;
   private String cacheJndiName;
   private byte[] clientChallenge;
   private SRPServerSession session;
   private Principal userPrincipal;
   private boolean loginFailed;
   
   public SRPCacheLoginModule()
   {
   }
   
   // --- Begin LoginModule interface methods
   /** Initialize the login module.
    @param subject, the subject to authenticate
    @param handler, the app CallbackHandler used to obtain username & password
    @param sharedState, used to propagate the authenticated principal and
    credential hash.
    @param options, the login module options. These include:
    cacheJndiName: the JNDI name of the CachePolicy of <Principal,Subject>
    information managed by the SRPSerice.
    domainName: the security domain name.
    */
   public void initialize(Subject subject, CallbackHandler handler, Map sharedState, Map options)
   {
      this.subject = subject;
      this.handler = handler;
      this.sharedState = sharedState;
      cacheJndiName = (String) options.get("cacheJndiName");
      log.trace("cacheJndiName="+cacheJndiName);
      domainName = (String) options.get("domainName");
   }
   
   /** Access the user Principal object and credentials by passing a
    SecurityAssociationCallback object to the registered CallbackHandler. This
    method then validates the user by looking up the cache information using
    the Principal object as the key and compares the cache credential against the
    the credential obtained from the SecurityAssociationCallback. The
    login credential should be the M1 verifcation challenge byte[].
    
    @return true is login succeeds, false if login does not apply.
    @exception LoginException, thrown on login failure.
    */
   public boolean login() throws LoginException
   {
      loginFailed = true;
      getUserInfo();
      
      String username = userPrincipal.getName();
      // First try to locate an SRPServerInterface using JNDI
      try
      {
         if( cacheJndiName == null )
            throw new LoginException("Required cacheJndiName option not set");
         InitialContext iniCtx = new InitialContext();
         CachePolicy cache = (CachePolicy) iniCtx.lookup(cacheJndiName);
         SRPSessionKey key;
         if( userPrincipal instanceof SRPPrincipal )
         {
            SRPPrincipal srpPrincpal = (SRPPrincipal) userPrincipal;
            key = new SRPSessionKey(username, srpPrincpal.getSessionID());
         }
         else
         {
            key = new SRPSessionKey(username);
         }
         Object cacheCredential = cache.get(key);
         if( cacheCredential == null )
         {
            throw new LoginException("No SRP session found for: "+key);
         }
         log.trace("Found SRP cache credential: "+cacheCredential);
         /** The cache object should be the SRPServerSession object used in the
          authentication of the client.
          */
         if( cacheCredential instanceof SRPServerSession )
         {
            session = (SRPServerSession) cacheCredential;
            if( validateCache(session) == false )
               throw new LoginException("Failed to validate SRP session key for: "+key);
         }
         else
         {
            throw new LoginException("Unknown type of cache credential: "+cacheCredential.getClass());
         }
      }
      catch(NamingException e)
      {
         log.error("Failed to load SRP auth cache", e);
         throw new LoginException("Failed to load SRP auth cache: "+e.toString(true));
      }
      
      log.trace("Login succeeded");
      // Put the username and the client challenge into the sharedState map
      sharedState.put("javax.security.auth.login.name", username);
      sharedState.put("javax.security.auth.login.password", clientChallenge);
      loginFailed = false;
      return true;
   }
   
   /** All login modules have completed the login() phase, commit if we
    succeeded. This entails adding the princial to the subject Principals set.
    It also adds the client challenge response to the PublicCredentials set
    and the private session key to the PrivateCredentials set as a SecretKeySpec.
    @return false, if the login() failed, true if the commit succeeds.
    @exception LoginException, thrown on failure to add the principal.
    */
   public boolean commit() throws LoginException
   {
      if( loginFailed == true )
         return false;
      Set principals = subject.getPrincipals();
      principals.add(userPrincipal);
      subject.getPublicCredentials().add(clientChallenge);
      byte[] sessionKey = session.getSessionKey();
      SRPParameters params = session.getParameters();
      Set privateCredentials = subject.getPrivateCredentials();
      privateCredentials.add(params);
      if( params.cipherAlgorithm != null )
      {
         SecretKeySpec secretKey = new SecretKeySpec(sessionKey, params.cipherAlgorithm);
         privateCredentials.add(secretKey);
      }
      else
      {
         privateCredentials.add(sessionKey);
      }

      return true;
   }

   public boolean abort() throws LoginException
   {
      userPrincipal = null;
      clientChallenge = null;
      return true;
   }

   /** Remove the userPrincipal, clientChallenge and sessionKey associated
    with the subject during commit().
    @return true always.
    @exception LoginException, thrown on exception during remove of the Principal
    added during the commit.
    */
   public boolean logout() throws LoginException
   {
      try
      {
         if( subject.isReadOnly() == false )
         {   // Remove userPrincipal
            Set s = subject.getPrincipals(userPrincipal.getClass());
            s.remove(userPrincipal);
            subject.getPublicCredentials().remove(clientChallenge);
            byte[] sessionKey = session.getSessionKey();
            SRPParameters params = session.getParameters();
            Set privateCredentials = subject.getPrivateCredentials();
            if( params.cipherAlgorithm != null )
            {
               SecretKeySpec secretKey = new SecretKeySpec(sessionKey, params.cipherAlgorithm);
               privateCredentials.remove(secretKey);
            }
            else
            {
               privateCredentials.remove(sessionKey);
            }
            privateCredentials.remove(params);
         }
      }
      catch(Exception e)
      {
         throw new LoginException("Failed to remove commit information, "+e.getMessage());
      }
      return true;
   }
   
// --- End LoginModule interface methods
   
   /** Obtain the Principal and credentials that are to be authenticated.
    */
   private void getUserInfo() throws LoginException
   {
      // Get the security association info
      if( handler == null )
         throw new LoginException("No CallbackHandler provied");

      SecurityAssociationCallback sac = new SecurityAssociationCallback();
      Callback[] callbacks = { sac };
      try
      {
         handler.handle(callbacks);
         userPrincipal = sac.getPrincipal();
         clientChallenge = (byte[]) sac.getCredential();
         sac.clearCredential();
      }
      catch(java.io.IOException e)
      {
         throw new LoginException(e.toString());
      }
      catch(UnsupportedCallbackException uce)
      {
         throw new LoginException("UnsupportedCallback: " + uce.getCallback().toString());
      }
      catch(ClassCastException e)
      {
         throw new LoginException("Credential info is not of type byte[], "+ e.getMessage());
      }
   }
   
   /** This method obtains the session getClientResponse() value which
    contains the challenge the client used to verify the session key.
    */
   private boolean validateCache(SRPServerSession session)
   {
      byte[] challenge = session.getClientResponse();
      boolean isValid = Arrays.equals(challenge, clientChallenge);
      return isValid;
   }
   
}
