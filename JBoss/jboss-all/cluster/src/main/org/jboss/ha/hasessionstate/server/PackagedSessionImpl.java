/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.hasessionstate.server;

import org.jboss.ha.hasessionstate.interfaces.PackagedSession;
import java.io.Serializable;

/**
 *   Default implementation of PackagedSession
 *
 *   @see PackagedSession, HASessionStateImpl
 *   @author sacha.labourey@cogito-info.ch
 *   @version $Revision: 1.1.4.3 $
 */
public class PackagedSessionImpl implements PackagedSession
{
   /** The serialVersionUID
    * @since 1.1.4.1
    */ 
   private static final long serialVersionUID = 4162160242862877223L;

   protected byte[] state;
   protected long versionId;
   protected String owner;
   protected Serializable key;
   protected transient long lastModificationTimeInVM = System.currentTimeMillis ();
   
   public PackagedSessionImpl () { }
   
   public PackagedSessionImpl (Serializable key, byte[] state, String owner)
   {
      this.key = key;
      this.setState (state);
      this.owner = owner;
   }
   
   public byte[] getState ()
   {
      return this.state;
   }
   
   public boolean setState (byte[] state)
   {
      this.lastModificationTimeInVM = System.currentTimeMillis ();
      if (isStateIdentical (state))
         return true;
      else
      {
         this.state = state;
         this.versionId++;
         return false;
      }
   }
   
   public boolean isStateIdentical (byte[] state)
   {
      return java.util.Arrays.equals (state, this.state);
   }
   
   public void update (PackagedSession clone)
   {
      this.state = (byte[])clone.getState().clone();
      this.versionId = clone.getVersion ();
      this.owner = clone.getOwner ();      
   }
   
   public String getOwner ()
   { return this.owner; }
   public void setOwner (String owner)
   { this.owner = owner; }
   
   public long getVersion ()
   { return this.versionId; }
   
   public Serializable getKey ()
   { return this.key; }
   public void setKey (Serializable key)
   { this.key = key; }

   public long unmodifiedExistenceInVM ()
   {
      return this.lastModificationTimeInVM;
   }
}
