package org.jboss.security.srp;

import java.io.Serializable;

/* An encapsulation of an SRP username and session id.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.3 $
 */
public class SRPSessionKey implements Serializable
{
   private static final long serialVersionUID = -7783783206948014409L;
   public static final Integer NO_SESSION_ID = new Integer(0);
   private String username;
   private int sessionID;

   public SRPSessionKey(String username)
   {
      this(username, NO_SESSION_ID);
   }
   public SRPSessionKey(String username, int sessionID)
   {
      this.username = username;
      this.sessionID = sessionID;
   }
   public SRPSessionKey(String username, Integer sessionID)
   {
      this.username = username;
      if( sessionID != null )
         this.sessionID = sessionID.intValue();
   }

   public boolean equals(Object obj)
   {
      SRPSessionKey key = (SRPSessionKey) obj;
      return this.username.equals(key.username) && this.sessionID == key.sessionID;
   }

   public int hashCode()
   {
      return this.username.hashCode() + this.sessionID;
   }

   public int getSessionID()
   {
      return sessionID;
   }

   public String getUsername()
   {
      return username;
   }

   public String toString()
   {
      return "{username="+username+", sessionID="+sessionID+"}";
   }
}
