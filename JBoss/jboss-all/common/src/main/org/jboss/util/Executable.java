/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.util;

/**
 * Interface for the execution of a task.
 * 
 * @see WorkerQueue
 * 
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.1 $
 */
public interface Executable
{
   /**
    * Executes the implemented task.
    */
   void execute() throws Exception;
}
