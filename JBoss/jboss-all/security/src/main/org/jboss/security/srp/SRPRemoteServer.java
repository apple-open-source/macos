/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp;

import java.io.IOException;
import java.rmi.RemoteException;
import java.rmi.server.RMIClientSocketFactory;
import java.rmi.server.RMIServerSocketFactory;
import java.rmi.server.UnicastRemoteObject;
import java.security.GeneralSecurityException;
import java.security.KeyException;
import java.security.NoSuchAlgorithmException;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import javax.crypto.Cipher;
import javax.crypto.SealedObject;

import org.jboss.logging.Logger;
import org.jboss.security.Util;
import org.jboss.security.srp.SRPVerifierStore.VerifierInfo;

/** An implementation of the RMI SRPRemoteServerInterface interface.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.5.4.4 $
 */
public class SRPRemoteServer extends UnicastRemoteObject implements SRPRemoteServerInterface
{
   private static Logger log = Logger.getLogger(SRPRemoteServer.class);
   private static int userSessionCount = 0;
   /** A map of <SRPSessionKey, SRPServerSession> for the active sessions */
   private Map sessionMap = Collections.synchronizedMap(new HashMap());

   /**
    * @supplierRole user password store
    * @clientRole remote access
    */
   private SRPVerifierStore verifierStore;
   private SRPServerListener listener;
   private boolean requireAuxChallenge;

   /** @link aggregation
    * @clientRole session container
    * @supplierRole server side info
    * @label created by getSRPParameters()*/
   /*#SRPServerSession lnkSRPServerSession;*/

   public SRPRemoteServer(SRPVerifierStore verifierStore) throws RemoteException
   {
      setVerifierStore(verifierStore);
   }

   public SRPRemoteServer(SRPVerifierStore verifierStore, int port) throws RemoteException
   {
      super(port);
      setVerifierStore(verifierStore);
   }

   public SRPRemoteServer(SRPVerifierStore verifierStore, int port, RMIClientSocketFactory csf,
         RMIServerSocketFactory ssf) throws RemoteException
   {
      super(port, csf, ssf);
      setVerifierStore(verifierStore);
   }

   /**
    */
   public void setVerifierStore(SRPVerifierStore verifierStore)
   {
      this.verifierStore = verifierStore;
      log.info("setVerifierStore, " + verifierStore);
   }

   public void addSRPServerListener(SRPServerListener listener)
   {
      this.listener = listener;
   }

   public void removeSRPServerListener(SRPServerListener listener)
   {
      if (this.listener == listener)
         this.listener = null;
   }

   public boolean getRequireAuxChallenge()
   {
      return this.requireAuxChallenge;
   }
   public void setRequireAuxChallenge(boolean flag)
   {
      this.requireAuxChallenge = flag;
   }

   /** The start of a new client session.
    */
   public SRPParameters getSRPParameters(String username)
         throws KeyException, RemoteException
   {
      Object[] params = this.getSRPParameters(username,false);
      SRPParameters srpParams = (SRPParameters) params[0];
      return srpParams;
   }

   public Object[] getSRPParameters(String username, boolean multipleSessions)
         throws KeyException, RemoteException
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("getSRPParameters, " + username);
      SRPParameters params = null;
      VerifierInfo info = null;
      try
      {
         info = verifierStore.getUserVerifier(username);
         if (info == null)
            throw new KeyException("Unknown username: " + username);
         params = new SRPParameters(info.N, info.g, info.salt,
            info.hashAlgorithm, info.cipherAlgorithm);
         if (log.isTraceEnabled())
         {
            log.trace("Params: " + params);
            byte[] hn = Util.newDigest().digest(params.N);
            log.trace("H(N): " + Util.tob64(hn));
            byte[] hg = Util.newDigest().digest(params.g);
            log.trace("H(g): " + Util.tob64(hg));
         }
         // Initialize the cipher IV
         if (params.cipherAlgorithm != null)
         {
            Cipher cipher = Cipher.getInstance(params.cipherAlgorithm);
            int size = cipher.getBlockSize();
            params.cipherIV = new byte[size];
            Util.nextBytes(params.cipherIV);
         }
      }
      catch (IOException e)
      {
         throw new RemoteException("Error during user info retrieval", e);
      }
      catch (KeyException e)
      {
         throw e;
      }
      catch (GeneralSecurityException e)
      {
         throw new RemoteException("Failed to init cipherIV", e);
      }
      catch (Throwable t)
      {
         log.error("Unexpected exception in getSRPParameters", t);
         throw new RemoteException("Unexpected exception in getSRPParameters", t);
      }

      // Generate a session id if the user may run multiple sessions
      Integer sessionID = SRPSessionKey.NO_SESSION_ID;
      if( multipleSessions == true )
         sessionID = nextSessionID();
      Object[] sessionInfo = {params, sessionID};
      // Create an SRP session
      SRPSessionKey key = new SRPSessionKey(username, sessionID);
      SRPServerSession session = new SRPServerSession(username, info.verifier,
            params);
      sessionMap.put(key, session);
      if( trace )
         log.trace("getSRPParameters, completed " + key);

      return sessionInfo;
   }

   public byte[] init(String username, byte[] A) throws SecurityException,
         NoSuchAlgorithmException, RemoteException
   {
      return this.init(username, A, 0);
   }
   public byte[] init(String username, byte[] A, int sessionID) throws SecurityException,
         NoSuchAlgorithmException, RemoteException
   {
      SRPSessionKey key = new SRPSessionKey(username, sessionID);
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("init, " + key);
      SRPServerSession session = (SRPServerSession) sessionMap.get(key);
      if (session == null)
         throw new SecurityException("Failed to find active session for username: " + username);

      byte[] B = session.exponential();
      session.buildSessionKey(A);
      if( trace )
         log.trace("init, completed "+key);
      return B;
   }

   public byte[] verify(String username, byte[] M1)
         throws SecurityException, RemoteException
   {
      return this.verify(username, M1, null, 0);
   }
   public byte[] verify(String username, byte[] M1, int sessionID)
         throws SecurityException, RemoteException
   {
      return this.verify(username, M1, null, sessionID);
   }

   /** Verify the session key hash. The client sends their username and M1
    hash to validate completion of the SRP handshake.

    @param username, the user ID by which the client is known. This is repeated to simplify
    the server session management.
    @param M1, the client hash of the session key; M1 = H(H(N) xor H(g) | H(U) | A | B | K)
    @param auxChallenge, an arbitrary addition data item that my be used as an additional
    challenge. One example usage would be to send a hardware generated token that was encrypted
    with the session private key for validation by the server.
    @return M2, the server hash of the client challenge; M2 = H(A | M1 | K)
    @throws SecurityException, thrown if M1 cannot be verified by the server
    @throws RemoteException, thrown by remote implementations
    */
   public byte[] verify(String username, byte[] M1, Object auxChallenge)
         throws SecurityException, RemoteException
   {
      return this.verify(username, M1, auxChallenge, 0);
   }
   public byte[] verify(String username, byte[] M1, Object auxChallenge, int sessionID)
         throws SecurityException, RemoteException
   {
      SRPSessionKey key = new SRPSessionKey(username, sessionID);
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("verify, " + key);
      SRPServerSession session = (SRPServerSession) sessionMap.get(key);
      if (session == null)
         throw new SecurityException("Failed to find active session for username: " + username);

      if (session.verify(M1) == false)
         throw new SecurityException("Failed to verify M1");

      /* If there is a auxChallenge have the verierStore verify the data
      */
      if( auxChallenge != null )
      {
         // See if this is an encrypted object
         if( auxChallenge instanceof SealedObject )
         {
            if( trace )
               log.trace("Decrypting sealed object");
            SRPParameters params = session.getParameters();
            Object challenge = null;
            try
            {
               challenge = Util.accessSealedObject(params.cipherAlgorithm, session.getSessionKey(),
                  params.cipherIV, auxChallenge);
            }
            catch (GeneralSecurityException e)
            {
               throw new RemoteException("Failed to access SealedObject", e);
            }
            auxChallenge = challenge;
         }
         if( trace )
            log.trace("Verifing aux challenge");
         this.verifierStore.verifyUserChallenge(username, auxChallenge);
      }
      else if( requireAuxChallenge == true )
      {
         throw new RemoteException("A non-null auxChallenge is required for verification");
      }

      // Inform the listener the user has been validated
      if (listener != null)
         listener.verifiedUser(key, session);
      if( trace )
         log.trace("verify, completed " + key);

      return session.getServerResponse();
   }

   /** Close the SRP session for the given username.
    */
   public void close(String username) throws SecurityException, RemoteException
   {
      this.close(username, 0);
   }
   public void close(String username, int sessionID) throws SecurityException, RemoteException
   {
      SRPSessionKey key = new SRPSessionKey(username, sessionID);
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("close, " + key);
      SRPServerSession session = (SRPServerSession) sessionMap.get(key);
      if (session == null)
         throw new SecurityException("Failed to find active session for username: " + username);
      if (listener != null)
         listener.closedUserSession(key);
      if( trace )
         log.trace("close, completed " + key);
   }

   private static synchronized Integer nextSessionID()
   {
      return new Integer(userSessionCount ++);
   }
}
