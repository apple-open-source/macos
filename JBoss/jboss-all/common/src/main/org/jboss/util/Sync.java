/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.util;

/**
 * Interface that gives synchronization semantic to implementors
 *
 * @see Semaphore
 * 
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.1 $
 */
public interface Sync 
{
   /**
    * Acquires this sync
    * 
    * @see #release
    */
   void acquire() throws InterruptedException;
   
   /**
    * Releases this sync
    * 
    * @see #acquire
    */
   void release();
}
