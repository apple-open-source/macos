/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp.jaas;

import java.rmi.Naming;
import java.security.Principal;
import java.util.Hashtable;
import java.util.Map;
import java.util.Set;
import java.util.ArrayList;
import java.io.Serializable;
import javax.naming.InitialContext;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.callback.Callback;
import javax.security.auth.callback.NameCallback;
import javax.security.auth.callback.PasswordCallback;
import javax.security.auth.callback.TextInputCallback;
import javax.security.auth.callback.UnsupportedCallbackException;
import javax.security.auth.login.LoginException;
import javax.security.auth.spi.LoginModule;

import org.jboss.logging.Logger;
import org.jboss.security.Util;
import org.jboss.security.auth.callback.ByteArrayCallback;
import org.jboss.security.srp.SRPClientSession;
import org.jboss.security.srp.SRPParameters;
import org.jboss.security.srp.SRPServerInterface;

/** A login module that uses the SRP protocol documented in RFC2945
 to authenticate a username & password in a secure fashion without
 using an encrypted channel.
 
 The supported configuration options include:
 <ul>
 <li>principalClassName: the fully qualified class name of the java.security.Principal
 implementation to use. The implementation must provide a public constructor that
 accepts a single String argument representing the name of the principal.
 If not specified this defaults to org.jboss.security.SimplePrincipal.
 </li>
 
 <li>srpServerJndiName: the JNDI name of the SRPServerInterface object to use
 for communicating with the SRP authentication server. If both srpServerJndiName
 and srpServerRmiUrl options are specified, the srpServerJndiName is tried before
 srpServerRmiUrl.
 </li>
 
 <li>srpServerRmiUrl: the RMI protocol URL string for the location of the
 SRPServerInterface proxy to use for communicating with the SRP authentication server.
 </li>
 </ul>
 
 This product uses the 'Secure Remote Password' cryptographic
 authentication system developed by Tom Wu (tjw@CS.Stanford.EDU).
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.6.4.4 $
 */
public class SRPLoginModule implements LoginModule
{
   private Subject subject;
   private CallbackHandler handler;
   private Map sharedState;
   private Hashtable jndiEnv;
   private String principalClassName;
   private String srpServerRmiUrl;
   private String srpServerJndiName;
   private String username;
   private char[] password;
   private SRPServerInterface srpServer;
   private SRPParameters params;
   private Principal userPrincipal;
   private Integer sessionID;
   private byte[] sessionKey;
   private byte[] abytes;
   private Object auxChallenge;
   private boolean externalRandomA;
   private boolean hasAuxChallenge;
   private boolean multipleSessions;
   private boolean loginFailed;
   private Logger log;
   
   /** Creates new SRPLoginModule */
   public SRPLoginModule()
   {
   }
   
   // --- Begin LoginModule interface methods
   /**
    @param subject, the subject to authenticate
    @param handler, the app CallbackHandler used to obtain username & password
    @param sharedState, used to propagate the authenticated principal and
    credential hash.
    @param options, the login module options. These include:
    principalClassName: the java.security.Principal class name implimentation to use.
    srpServerJndiName: the jndi name of the SRPServerInterface implimentation to use. This
    is tried before srpServerRmiUrl.
    srpServerRmiUrl: the rmi url for the SRPServerInterface implimentation to use.
    externalRandomA: a true/false flag indicating if the random component of
      the client public key A should come from the user callback.
    hasAuxChallenge: A true/false flag indicating an that a string will be sent to the
    server as an additional challenge for the server to validate. If the client session
    supports an encryption cipher then a temporary cipher will be created and the challenge
    object sent as a SealedObject.
    multipleSessions: a true/false flag indicating if a given client may have multiple
    SRP login session active simultaneously.
    */
   public void initialize(Subject subject, CallbackHandler handler, Map sharedState,
      Map options)
   {
      this.jndiEnv = new Hashtable(options);
      this.subject = subject;
      this.handler = handler;
      this.sharedState = sharedState;
      principalClassName = (String) options.get("principalClassName");
      srpServerJndiName = (String) options.get("srpServerJndiName");
      srpServerRmiUrl = (String) options.get("srpServerRmiUrl");
      String tmp = (String) options.get("externalRandomA");
      if( tmp != null )
         externalRandomA = Boolean.valueOf(tmp).booleanValue();
      multipleSessions = false;
      tmp = (String) options.get("multipleSessions");
      if( tmp != null )
         multipleSessions = Boolean.valueOf(tmp).booleanValue();
      tmp = (String) options.get("hasAuxChallenge");
      if( tmp != null )
         hasAuxChallenge = Boolean.valueOf(tmp).booleanValue();

      /* Remove all standard options and if there are any left, use these as
         the JNDI InitialContext env
      */
      jndiEnv.remove("principalClassName");
      jndiEnv.remove("srpServerJndiName");
      jndiEnv.remove("srpServerRmiUrl");
      jndiEnv.remove("externalRandomA");
      jndiEnv.remove("multipleSessions");
      jndiEnv.remove("hasAuxChallenge");

      log = Logger.getLogger(getClass());
   }

   /** This is where the SRP protocol exchange occurs.
    @return true is login succeeds, false if login does not apply.
    @exception LoginException, thrown on login failure.
    */
   public boolean login() throws LoginException
   {
      boolean trace = log.isTraceEnabled();
      loginFailed = true;
      getUserInfo();
      // First try to locate an SRPServerInterface using JNDI
      if( srpServerJndiName != null )
      {
         srpServer = loadServerFromJndi(srpServerJndiName);
      }
      else if( srpServerRmiUrl != null )
      {
         srpServer = loadServer(srpServerRmiUrl);
      }
      else
      {
         throw new LoginException("No option specified to access a SRPServerInterface instance");
      }
      if( srpServer == null )
         throw new LoginException("Failed to access a SRPServerInterface instance");
      
      byte[] M1, M2;
      SRPClientSession client = null;
      try
      {   // Perform the SRP login protocol
         if( trace )
            log.trace("Getting SRP parameters for username: "+username);
         Util.init();
         Object[] sessionInfo = srpServer.getSRPParameters(username, multipleSessions);
         params = (SRPParameters) sessionInfo[0];
         sessionID = (Integer) sessionInfo[1];
         if( sessionID == null )
            sessionID = new Integer(0);
         if( trace )
         {
            log.trace("SessionID: "+sessionID);
            log.trace("N: "+Util.tob64(params.N));
            log.trace("g: "+Util.tob64(params.g));
            log.trace("s: "+Util.tob64(params.s));
            log.trace("cipherAlgorithm: "+params.cipherAlgorithm);
            log.trace("hashAlgorithm: "+params.hashAlgorithm);
         }
         byte[] hn = Util.newDigest().digest(params.N);
         if( trace )
            log.trace("H(N): "+Util.tob64(hn));
         byte[] hg = Util.newDigest().digest(params.g);
         if( trace )
         {
            log.trace("H(g): "+Util.tob64(hg));
            log.trace("Creating SRPClientSession");
         }

         if( abytes != null )
            client = new SRPClientSession(username, password, params, abytes);
         else
            client = new SRPClientSession(username, password, params);
         if( trace )
            log.trace("Generating client public key");

         byte[] A = client.exponential();
         if( trace )
            log.trace("Exchanging public keys");
         byte[] B = srpServer.init(username, A, sessionID.intValue());
         if( trace )
            log.trace("Generating server challenge");
         M1 = client.response(B);

         if( trace )
            log.trace("Exchanging challenges");
         sessionKey = client.getSessionKey();
         if( auxChallenge != null )
         {
            auxChallenge = encryptAuxChallenge(auxChallenge, params.cipherAlgorithm,
                  params.cipherIV, sessionKey);
            M2 = srpServer.verify(username, M1, auxChallenge, sessionID.intValue());
         }
         else
         {
            M2 = srpServer.verify(username, M1, sessionID.intValue());
         }
      }
      catch(Exception e)
      {
         log.warn("Failed to complete SRP login", e);
         throw new LoginException("Failed to complete SRP login, msg="+e.getMessage());
      }

      if( trace )
         log.trace("Verifying server response");
      if( client.verify(M2) == false )
         throw new LoginException("Failed to validate server reply");
      if( trace )
         log.trace("Login succeeded");
      
      // Put the username and the client challenge into the sharedState map
      sharedState.put("javax.security.auth.login.name", username);
      sharedState.put("javax.security.auth.login.password", M1);
      loginFailed = false;
      return true;
   }

   /** All login modules have completed the login() phase, comit if we
    succeeded. This entails adding an instance of principalClassName to the
    subject principals set and the private session key to the PrivateCredentials
    set.
    
    @return false, if the login() failed, true if the commit succeeds.
    @exception LoginException, thrown on failure to create a Principal.
    */
   public boolean commit() throws LoginException
   {
      if( loginFailed == true )
         return false;
      
      // Associate an instance of Principal with the subject
      userPrincipal = new SRPPrincipal(username, sessionID);
      subject.getPrincipals().add(userPrincipal);
      Set privateCredentials = subject.getPrivateCredentials();
      privateCredentials.add(sessionKey);
      if( sessionID != null )
         privateCredentials.add(sessionID);
      if( params.cipherAlgorithm != null )
      {
         Object secretKey = createSecretKey(params.cipherAlgorithm, sessionKey);
         privateCredentials.add(secretKey);
      }
      privateCredentials.add(params);

      return true;
   }

   public boolean abort() throws LoginException
   {
      username = null;
      password = null;
      return true;
   }

   /** Remove the userPrincipal associated with the subject.
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
            subject.getPrivateCredentials().remove(sessionKey);
         }
         if( srpServer != null )
         {
            srpServer.close(username, sessionID.intValue());
         }
      }
      catch(Exception e)
      {
         throw new LoginException("Failed to remove user principal, "+e.getMessage());
      }
      return true;
   }

// --- End LoginModule interface methods

   private void getUserInfo() throws LoginException
   {
      // See if there is a shared username & password
      String _username = (String) sharedState.get("javax.security.auth.login.name");
      char[] _password = null;
      if( username != null )
      {
         Object pw = sharedState.get("javax.security.auth.login.password");
         if( pw instanceof char[] )
            _password = (char[]) pw;
         else if( pw != null )
            _password = pw.toString().toCharArray();
      }
      
      // If we have a username, password return
      if( _username != null && _password != null )
      {
         username = _username;
         password = _password;
         return;
      }
      
      // Request a username and password
      if( handler == null )
         throw new LoginException("No CallbackHandler provied to SRPLoginModule");
      
      NameCallback nc = new NameCallback("Username: ", "guest");
      PasswordCallback pc = new PasswordCallback("Password: ", false);
      ByteArrayCallback bac = new ByteArrayCallback("Public key random number: ");
      TextInputCallback tic = new TextInputCallback("Auxillary challenge token: ");
      ArrayList tmpList = new ArrayList();
      tmpList.add(nc);
      tmpList.add(pc);
      if( externalRandomA == true )
         tmpList.add(bac);
      if( hasAuxChallenge == true )
         tmpList.add(tic);
      Callback[] callbacks = new Callback[tmpList.size()];
      tmpList.toArray(callbacks);
      try
      {
         handler.handle(callbacks);
         username = nc.getName();
         _password = pc.getPassword();
         if( _password != null )
            password = _password;
         pc.clearPassword();
         if( externalRandomA == true )
            abytes = bac.getByteArray();
         if( hasAuxChallenge == true )
            this.auxChallenge = tic.getText();
      }
      catch(java.io.IOException e)
      {
         throw new LoginException(e.toString());
      }
      catch(UnsupportedCallbackException uce)
      {
         throw new LoginException("UnsupportedCallback: " + uce.getCallback().toString());
      }
   }

   private SRPServerInterface loadServerFromJndi(String jndiName)
   {
      SRPServerInterface server = null;
      try
      {
         InitialContext ctx = new InitialContext(jndiEnv);
         server = (SRPServerInterface) ctx.lookup(jndiName);
      }
      catch(Exception e)
      {
         log.error("Failed to lookup("+jndiName+")", e);
      }
      return server;
   }
   private SRPServerInterface loadServer(String rmiUrl)
   {
      SRPServerInterface server = null;
      try
      {
         server = (SRPServerInterface) Naming.lookup(rmiUrl);
      }
      catch(Exception e)
      {
         log.error("Failed to lookup("+rmiUrl+")", e);
      }
      return server;
   }

   /** If there is a cipher algorithm and JCE is available, encrypt the challenge, else
    * just return the challenge as the raw object.
    */
   private Object encryptAuxChallenge(Object challenge, String cipherAlgorithm,
      byte[] cipherIV, Object key)
      throws LoginException
   {
      if( cipherAlgorithm == null )
         return challenge;
      Object sealedObject = null;
      try
      {
         Serializable data = (Serializable) challenge;
         Object tmpKey = Util.createSecretKey(cipherAlgorithm, key);
         sealedObject = Util.createSealedObject(cipherAlgorithm, tmpKey, cipherIV, data);
      }
      catch(Exception e)
      {
         log.error("Failed to encrypt aux challenge", e);
         throw new LoginException("Failed to encrypt aux challenge");
      }
      return sealedObject;
   }

   /** Use reflection to create a javax.crypto.spec.SecretKeySpec to avoid
    an explicit reference to SecretKeySpec so that the JCE is not needed
    unless the SRP parameters indicate that encryption is needed.
   */
   private Object createSecretKey(String cipherAlgorithm, Object key) throws LoginException
   {
      Object secretKey = null;
      try
      {
         secretKey = Util.createSecretKey(cipherAlgorithm, key);
      }
      catch(Exception e)
      {
         log.error("Failed to create SecretKey", e);
         throw new LoginException("Failed to create SecretKey");
      }
      return secretKey;
   }
}
