/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp;

import java.rmi.RemoteException;
import java.security.KeyException;
import java.security.NoSuchAlgorithmException;

/** An interface describing the message exchange of the SRP protocol as
described in RFC2945. This is an RMI compatible interface in that all methods
declare that they throw a RemoteException, but it does not extend from
java.rmi.Remote so that it cannot be used in place of a Remote object.
For an RMI interface see the SRPRemoteServerInterface.

There are two versions of each method. One that takes an arbitrary session number
and one that does not. The session number form allows a user to maintain mutiple
SRP sessions.

@see org.jboss.security.srp.SRPRemoteServerInterface

@author Scott.Stark@jboss.org
@version $Revision: 1.5.4.2 $
*/
public interface SRPServerInterface
{
    /** Get the SRP parameters to use for this session.
     * @param username, the SRP username
     * @return the users SRPParameters object
    */
    public SRPParameters getSRPParameters(String username) throws KeyException, RemoteException;
    /** Get the SRP parameters to use for this session and create an arbitrary session id
     * to allow for multiple SRP sessions for this user.
     * @param username, the SRP username
     * @param mutipleSessions, a flag that if true indicates the user may initiate mutiple
     * sessions and an arbitrary session id will be created.
     * @return an array of {SRPParameters, Integer} where element[0] is the SRPParameters
     * object and element[1] is the session id as an Integer.
    */
    public Object[] getSRPParameters(String username, boolean mutipleSessions)
      throws KeyException, RemoteException;

    /** Initiate the SRP algorithm. The client sends their username and the
     public key A to begin the SRP handshake.
    @param username, the user ID by which the client is known.
    @param A, the client public key = (g ^ a) % N
    @return byte[], ephemeral server public key B = (v + g ^ b) % N
    @throws KeyException, thrown if the username is not known by the server.
    @throws RemoteException, thrown by remote implementations
    */
    public byte[] init(String username, byte[] A) throws SecurityException,
      NoSuchAlgorithmException, RemoteException;
    /** Initiate the SRP algorithm. The client sends their username and the
     public key A to begin the SRP handshake.
    @param username, the user ID by which the client is known.
    @param A, the client public key = (g ^ a) % N
    @param sessionID, the arbitrary session id obtained from getSRPParameters. A 0
     indicates there is no sessionID.
    @return byte[], ephemeral server public key B = (v + g ^ b) % N
    @throws KeyException, thrown if the username is not known by the server.
    @throws RemoteException, thrown by remote implementations
    */
    public byte[] init(String username, byte[] A, int sessionID) throws SecurityException,
      NoSuchAlgorithmException, RemoteException;

    /** Verify the session key hash. The client sends their username and M1
     hash to validate completion of the SRP handshake.

    @param username, the user ID by which the client is known. This is repeated to simplify
        the server session management.
    @param M1, the client hash of the session key; M1 = H(H(N) xor H(g) | H(U) | A | B | K)
    @return M2, the server hash of the client challenge; M2 = H(A | M1 | K)
    @throws SecurityException, thrown if M1 cannot be verified by the server
    @throws RemoteException, thrown by remote implementations
    */
    public byte[] verify(String username, byte[] M1) throws SecurityException, RemoteException;
    public byte[] verify(String username, byte[] M1, int sessionID)
          throws SecurityException, RemoteException;

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
          throws SecurityException, RemoteException;
    public byte[] verify(String username, byte[] M1, Object auxChallenge, int sessionID)
          throws SecurityException, RemoteException;

    /** Close the SRP session for the given username.
     */
    public void close(String username) throws SecurityException, RemoteException;
    public void close(String username, int sessionID) throws SecurityException, RemoteException;
}
