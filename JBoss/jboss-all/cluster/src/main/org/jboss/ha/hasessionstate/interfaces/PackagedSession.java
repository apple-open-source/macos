/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.hasessionstate.interfaces;


import java.io.Serializable;

/**
 *   Information about a session that is shared by nodes in a same sub-partition
 *
 *   @see HASessionState, PackagedSessionImpl
 *   @author sacha.labourey@cogito-info.ch
 *   @version $Revision: 1.2.4.1 $
 *
 * <p><b>Revisions:</b><br>
 */

public interface PackagedSession extends Serializable
{
   /** The serialVersionUID
    * @since 1.2
    */ 
   static final long serialVersionUID = 689622988452110553L;
   /*
    * Stored state
    */
   public byte[] getState ();
   public boolean setState (byte[] state);
   
   /*
    * Stored state
    */   
   public boolean isStateIdentical (byte[] state);
   
   /*
    * Update the state and content of this PackagedSession from the content of another
    * PackagedSession.
    */
   public void update (PackagedSession clone);
   
   /*
    * Owner node of the state
    */
   public String getOwner ();
   public void setOwner (String owner);
   
   /*
    * Version number of this state
    */
   public long getVersion ();
   
   /*
    * Key identifier associated with this state
    */
   public Serializable getKey ();
   public void setKey (Serializable key);
   
   /*
    * Number of miliseconds since when this state has not been modified in this VM
    */
   public long unmodifiedExistenceInVM ();
}
