package org.jboss.security.srp;

/** A callback interface for SRP session events.

@author  Scott.Stark@jboss.org
@version $Revision: 1.1.6.1 $
*/
public interface SRPServerListener
{
   /** Called when a user has successfully completed the SRP handshake and any auxillary
    * challenge verification.
    * @param key, the {username, sessionID} pair
    * @param session, the server SRP session information
    */
   public void verifiedUser(SRPSessionKey key, SRPServerSession session);
   /** Called when a user requests that a session be closed
    *
    * @param key, the {username, sessionID} pair
    */
   public void closedUserSession(SRPSessionKey key);
}
