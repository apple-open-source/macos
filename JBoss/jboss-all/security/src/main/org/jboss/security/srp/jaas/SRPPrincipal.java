/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp.jaas;

import org.jboss.security.SimplePrincipal;

/** An extension of SimplePrincipal that adds the SRP session ID
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class SRPPrincipal extends SimplePrincipal
{
   /** Serial Version */
   static final long serialVersionUID = -7123071794402068344L;
   /** The SRP session ID, 0 == no session */
   private int sessionID;

   /** Creates a new instance of SRPPrincipal */
   public SRPPrincipal(String name)
   {
      this(name, 0);
   }
   public SRPPrincipal(String name, int sessionID)
   {
      super(name);
      this.sessionID = sessionID;
   }
   public SRPPrincipal(String name, Integer sessionID)
   {
      super(name);
      this.sessionID = sessionID != null ? sessionID.intValue() : 0;
   }

   public int getSessionID()
   {
      return sessionID;
   }
}
