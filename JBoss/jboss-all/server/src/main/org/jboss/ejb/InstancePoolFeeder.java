/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb;

import org.jboss.metadata.XmlLoadable;
import org.jboss.ejb.InstancePool;

/**
 * Interface for bean instances Pool Feeder
 *
 * @author <a href="mailto:vincent.harcq@hubmethods.com">Vincent Harcq</a>
 * @version $Revision: 1.2 $
 */
public interface InstancePoolFeeder
   extends XmlLoadable
{
   /**
    * Start the pool feeder.
    */
   void start();

   /**
    * Stop the pool feeder.
    */
   void stop();

   /**
    * Sets the instance pool inside the pool feeder.
    *
    * @param ip the instance pool
    */
   void setInstancePool(InstancePool ip);

   /**
    * Tells if the pool feeder is already started.
    * The reason is that we start the PF at first get() on the pool and we
    * want to give a warning to the user when the pool is empty.
    *
    * @return true if started
    */
   boolean isStarted();
}
