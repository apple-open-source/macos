/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.jaws;

/**
 * Interface for JAWSPersistenceManager Command Factories 
 *    
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @version $Revision: 1.6 $
 */
public interface JPMCommandFactory
{
   // Public --------------------------------------------------------
   
   //command factory lifecycle commands

   void destroy();

   // lifecycle commands
   
   public JPMInitCommand createInitCommand();
   
   public JPMStartCommand createStartCommand();
   
   public JPMStopCommand createStopCommand();
   
   public JPMDestroyCommand createDestroyCommand();
   
   // entity persistence-related commands
   
   public JPMFindEntityCommand createFindEntityCommand();
   
   public JPMFindEntitiesCommand createFindEntitiesCommand();
   
   public JPMCreateEntityCommand createCreateEntityCommand();
   
   public JPMRemoveEntityCommand createRemoveEntityCommand();
   
   public JPMLoadEntityCommand createLoadEntityCommand();
   
   public JPMStoreEntityCommand createStoreEntityCommand();
   
   // entity activation and passivation commands
   
   public JPMActivateEntityCommand createActivateEntityCommand();
   
   public JPMPassivateEntityCommand createPassivateEntityCommand();
}
