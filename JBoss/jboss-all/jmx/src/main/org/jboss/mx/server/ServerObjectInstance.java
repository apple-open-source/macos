/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.server;

import java.io.ObjectStreamException;

import javax.management.ObjectInstance;
import javax.management.ObjectName;

/**
 * An Object Instance that differentiates between MBeanServers.
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1 $
 */
public class ServerObjectInstance
   extends ObjectInstance
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The agent id
    */
   String agentID;

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Create a new Server Object Instance
    * 
    * @param name the object name
    * @param className the class name
    * @param agentID the agentID
    */
   public ServerObjectInstance(ObjectName name, String className, String agentID)
   {
      super(name, className);
      this.agentID = agentID;
   }

   // Public ------------------------------------------------------

   /**
    * Retrieve the agent id of the object instance
    *
    * @return the agent id
    */
   String getAgentID()
   {
      return agentID;
   }

   // X implementation --------------------------------------------

   // ObjectInstance overrides ------------------------------------

   public boolean equals(Object object)
   {
     if (object instanceof ServerObjectInstance)
       return (super.equals(object) == true 
               && this.agentID.equals(((ServerObjectInstance)object).agentID));
     else
       return super.equals(object);
   }

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   /**
    * We replace ourself with an ObjectInstance in the stream.
    * This loses the agentId which isn't part of the spec.
    *
    * @return an ObjectInstance version of ourself
    * @exception ObjectStreamException for a serialization error
    */
   private Object writeReplace()
      throws ObjectStreamException
   {
      return new ObjectInstance(getObjectName(), getClassName());
   }

   // Inner classes -----------------------------------------------
}
