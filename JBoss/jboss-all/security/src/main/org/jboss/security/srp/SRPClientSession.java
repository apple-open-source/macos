/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp;

import java.math.BigInteger;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

import org.jboss.logging.Logger;
import org.jboss.security.Util;

/** The client side logic to the SRP protocol. The class is intended to be used
 * with a SRPServerSession object via the SRPServerInterface. The SRP algorithm
 * using these classes consists of:
 *
 * 1. Get server, SRPServerInterface server = (SRPServerInterface) Naming.lookup(...);
 * 2. Get SRP parameters, SRPParameters params = server.getSRPParameters(username);
 * 3. Create a client session, SRPClientSession client = new SRPClientSession(username,
 * password, params);
 * 4. Exchange public keys, byte[] A = client.exponential();
 * byte[] B = server.init(username, A);
 * 5. Exchange challenges, byte[] M1 = client.response(B);
 * byte[] M2 = server.verify(username, M1);
 * 6. Verify the server response, if( client.verify(M2) == false )
 * throw new SecurityException("Failed to validate server reply");
 * 7. Validation complete
 *
 * Note that these steps are stateful. They must be performed in order and a
 * step cannot be repeated to update the session state.
 *
 * This product uses the 'Secure Remote Password' cryptographic
 * authentication system developed by Tom Wu (tjw@CS.Stanford.EDU).
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.5.4.2 $
 */
public class SRPClientSession
{
   private static Logger log = Logger.getLogger(SRPClientSession.class);
   private SRPParameters params;
   private BigInteger N;
   private BigInteger g;
   private BigInteger x;
   private BigInteger v;
   private byte[] s;
   private BigInteger a;
   private BigInteger A;
   private byte[] K;
   /** The M1 = H(H(N) xor H(g) | H(U) | s | A | B | K) hash */
   private MessageDigest clientHash;
   /** The M2 = H(A | M | K) hash */
   private MessageDigest serverHash;
   
   private static int A_LEN = 64;
   
   /** Creates a new SRP server session object from the username, password
    verifier,
    @param username, the user ID
    @param password, the user clear text password
    @param params, the SRP parameters for the session
    */
   public SRPClientSession(String username, char[] password, SRPParameters params)
   {
      this(username, password, params, null);
   }

   /** Creates a new SRP server session object from the username, password
    verifier,
    @param username, the user ID
    @param password, the user clear text password
    @param params, the SRP parameters for the session
    @param abytes, the random exponent used in the A public key. This must be
      8 bytes in length.
    */
   public SRPClientSession(String username, char[] password, SRPParameters params,
      byte[] abytes)
   {
      try
      {
         // Initialize the secure random number and message digests
         Util.init();
      }
      catch(NoSuchAlgorithmException e)
      {
      }
      this.params = params;
      this.g = new BigInteger(1, params.g);
      this.N = new BigInteger(1, params.N);
      if( abytes != null )
      {
         if( 8*abytes.length != A_LEN )
            throw new IllegalArgumentException("The abytes param must be "
               +(A_LEN/8)+" in length, abytes.length="+abytes.length);
         this.a = new BigInteger(abytes);
      }

      if( log.isTraceEnabled() )
         log.trace("g: "+Util.tob64(params.g));
      // Calculate x = H(s | H(U | ':' | password))
      byte[] xb = Util.calculatePasswordHash(username, password, params.s);
      if( log.isTraceEnabled() )
         log.trace("x: "+Util.tob64(xb));
      this.x = new BigInteger(1, xb);
      this.v = g.modPow(x, N);  // g^x % N
      if( log.isTraceEnabled() )
         log.trace("v: "+Util.tob64(v.toByteArray()));
      
      serverHash = Util.newDigest();
      clientHash = Util.newDigest();
      // H(N)
      byte[] hn = Util.newDigest().digest(params.N);
      if( log.isTraceEnabled() )
         log.trace("H(N): "+Util.tob64(hn));
      // H(g)
      byte[] hg = Util.newDigest().digest(params.g);
      if( log.isTraceEnabled() )
         log.trace("H(g): "+Util.tob64(hg));
      // clientHash = H(N) xor H(g)
      byte[] hxg = Util.xor(hn, hg, 20);
      if( log.isTraceEnabled() )
         log.trace("H(N) xor H(g): "+Util.tob64(hxg));
      clientHash.update(hxg);
      if( log.isTraceEnabled() )
      {
         MessageDigest tmp = Util.copy(clientHash);
         log.trace("H[H(N) xor H(g)]: "+Util.tob64(tmp.digest()));
      }
      // clientHash = H(N) xor H(g) | H(U)
      clientHash.update(Util.newDigest().digest(username.getBytes()));
      if( log.isTraceEnabled() )
      {
         MessageDigest tmp = Util.copy(clientHash);
         log.trace("H[H(N) xor H(g) | H(U)]: "+Util.tob64(tmp.digest()));
      }
      // clientHash = H(N) xor H(g) | H(U) | s
      clientHash.update(params.s);
      if( log.isTraceEnabled() )
      {
         MessageDigest tmp = Util.copy(clientHash);
         log.trace("H[H(N) xor H(g) | H(U) | s]: "+Util.tob64(tmp.digest()));
      }
      K = null;
   }
   
   /**
    * @returns The exponential residue (parameter A) to be sent to the server.
    */
   public byte[] exponential()
   {
      byte[] Abytes = null;
      if(A == null)
      {
         /* If the random component of A has not been specified use a random
         number */
         if( a == null )
         {
            BigInteger one = BigInteger.ONE;
            do
            {
               a = new BigInteger(A_LEN, Util.getPRNG());
            } while(a.compareTo(one) <= 0);
         }
         A = g.modPow(a, N);
         Abytes = Util.trim(A.toByteArray());
         // clientHash = H(N) xor H(g) | H(U) | A
         clientHash.update(Abytes);
         if( log.isTraceEnabled() )
         {
            MessageDigest tmp = Util.copy(clientHash);
            log.trace("H[H(N) xor H(g) | H(U) | s | A]: "+Util.tob64(tmp.digest()));
         }
         // serverHash = A
         serverHash.update(Abytes);
      }
      return Abytes;
   }
   
   /**
    @returns M1 = H(H(N) xor H(g) | H(U) | s | A | B | K)
    @exception NoSuchAlgorithmException thrown if the session key
    MessageDigest algorithm cannot be found.
    */
   public byte[] response(byte[] Bbytes) throws NoSuchAlgorithmException
   {
      // clientHash = H(N) xor H(g) | H(U) | s | A | B
      clientHash.update(Bbytes);
      if( log.isTraceEnabled() )
      {
         MessageDigest tmp = Util.copy(clientHash);
         log.trace("H[H(N) xor H(g) | H(U) | s | A | B]: "+Util.tob64(tmp.digest()));
      }
      // Calculate u as the first 32 bits of H(B)
      byte[] hB = Util.newDigest().digest(Bbytes);
      byte[] ub =
      {hB[0], hB[1], hB[2], hB[3]};
      // Calculate S = (B - g^x) ^ (a + u * x) % N
      BigInteger B = new BigInteger(1, Bbytes);
      if( log.isTraceEnabled() )
         log.trace("B: "+Util.tob64(B.toByteArray()));
      if( B.compareTo(v) < 0 )
         B = B.add(N);
      if( log.isTraceEnabled() )
         log.trace("B': "+Util.tob64(B.toByteArray()));
      if( log.isTraceEnabled() )
         log.trace("v: "+Util.tob64(v.toByteArray()));
      BigInteger u = new BigInteger(1, ub);
      if( log.isTraceEnabled() )
         log.trace("u: "+Util.tob64(u.toByteArray()));
      BigInteger B_v = B.subtract(v);
      if( log.isTraceEnabled() )
         log.trace("B - v: "+Util.tob64(B_v.toByteArray()));
      BigInteger a_ux = a.add(u.multiply(x));
      if( log.isTraceEnabled() )
         log.trace("a + u * x: "+Util.tob64(a_ux.toByteArray()));
      BigInteger S = B_v.modPow(a_ux, N);
      if( log.isTraceEnabled() )
         log.trace("S: "+Util.tob64(S.toByteArray()));
      // K = SessionHash(S)
      MessageDigest sessionDigest = MessageDigest.getInstance(params.hashAlgorithm);
      K = sessionDigest.digest(S.toByteArray());
      if( log.isTraceEnabled() )
         log.trace("K: "+Util.tob64(K));
      // clientHash = H(N) xor H(g) | H(U) | A | B | K
      clientHash.update(K);
      byte[] M1 = clientHash.digest();
      if( log.isTraceEnabled() )
         log.trace("M1: H[H(N) xor H(g) | H(U) | s | A | B | K]: "+Util.tob64(M1));
      serverHash.update(M1);
      serverHash.update(K);
      if( log.isTraceEnabled() )
      {
         MessageDigest tmp = Util.copy(serverHash);
         log.trace("H[A | M1 | K]: "+Util.tob64(tmp.digest()));
      }
      return M1;
   }
   /**
    * @param M2 The server's response to the client's challenge
    * @returns True if and only if the server's response was correct.
    */
   public boolean verify(byte[] M2)
   {
      // M2 = H(A | M1 | K)
      byte[] myM2 = serverHash.digest();
      boolean valid = Arrays.equals(M2, myM2);
      if( log.isTraceEnabled() )
      {
         log.trace("verify serverM2: "+Util.tob64(M2));
         log.trace("verify M2: "+Util.tob64(myM2));
      }
      return valid;
   }
   
   /** Returns the negotiated session K, K = SHA_Interleave(S)
    @return the private session K byte[]
    @throws SecurityException - if the current thread does not have an
    getSessionKey SRPPermission.
    */
   public byte[] getSessionKey() throws SecurityException
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
      {
         SRPPermission p = new SRPPermission("getSessionKey");
         sm.checkPermission(p);
      }
      return K;
   }
}
