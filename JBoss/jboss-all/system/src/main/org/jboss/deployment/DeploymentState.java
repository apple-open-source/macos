
package org.jboss.deployment;

import java.io.ObjectStreamException;
import java.io.Serializable;

/** A type-safe enumeration for the status a DeploymentInfo may be in
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public class DeploymentState implements Serializable
{
   public static final DeploymentState CONSTRUCTED = new DeploymentState("CONSTRUCTED");
   public static final DeploymentState INIT_WAITING_DEPLOYER = new DeploymentState("INIT_WAITING_DEPLOYER");
   public static final DeploymentState INIT_HAS_DEPLOYER = new DeploymentState("INIT_HAS_DEPLOYER");
   public static final DeploymentState INIT_DEPLOYER = new DeploymentState("INIT_DEPLOYER");
   public static final DeploymentState INITIALIZED = new DeploymentState("INITIALIZED");

   public static final DeploymentState CREATE_SUBDEPLOYMENTS = new DeploymentState("CREATE_SUBDEPLOYMENTS");
   public static final DeploymentState CREATE_DEPLOYER = new DeploymentState("CREATE_DEPLOYER");
   public static final DeploymentState CREATED = new DeploymentState("CREATED");

   public static final DeploymentState START_SUBDEPLOYMENTS = new DeploymentState("START_SUBDEPLOYMENTS");
   public static final DeploymentState START_DEPLOYER = new DeploymentState("START_DEPLOYER");
   public static final DeploymentState STARTED = new DeploymentState("STARTED");

   public static final DeploymentState STOPPED = new DeploymentState("STOPPED");
   public static final DeploymentState DESTROYED = new DeploymentState("DESTROYED");
   public static final DeploymentState FAILED = new DeploymentState("FAILED");

   private String state;
   private DeploymentState(String state)
   {
      this.state = state;
   }

   /** A factory to translate a string into the corresponding DeploymentState.
    */
   public static DeploymentState getDeploymentState(String state)
   {
      DeploymentState theState = null;
      state = state.toUpperCase();
      if( state.equals("CONSTRUCTED") )
         theState = CONSTRUCTED;
      else if( state.equals("INIT_WAITING_DEPLOYER") )
         theState = INIT_WAITING_DEPLOYER;
      else if( state.equals("INIT_HAS_DEPLOYER") )
         theState = INIT_HAS_DEPLOYER;
      else if( state.equals("INIT_DEPLOYER") )
         theState = INIT_DEPLOYER;
      else if( state.equals("INITIALIZED") )
         theState = INITIALIZED;
      else if( state.equals("CREATE_SUBDEPLOYMENTS") )
         theState = CREATE_SUBDEPLOYMENTS;
      else if( state.equals("CREATE_DEPLOYER") )
         theState = CREATE_DEPLOYER;
      else if( state.equals("CREATED") )
         theState = CREATED;
      else if( state.equals("START_SUBDEPLOYMENTS") )
         theState = START_SUBDEPLOYMENTS;
      else if( state.equals("START_DEPLOYER") )
         theState = START_DEPLOYER;
      else if( state.equals("STARTED") )
         theState = STARTED;
      else if( state.equals("STOPPED") )
         theState = STOPPED;
      else if( state.equals("DESTROYED") )
         theState = DESTROYED;
      else if( state.equals("FAILED") )
         theState = FAILED;

      return theState;
   }

   public String toString()
   {
      return state;
   }

   /** Resolve objects on deserialization to one of the identity objects.
   */
   private Object readResolve() throws ObjectStreamException
   {
      Object identity = getDeploymentState(state);
      return identity;
   }

}
