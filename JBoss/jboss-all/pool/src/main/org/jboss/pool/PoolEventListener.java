/*
 * Licensed under the X license (see http://www.x.org/terms.htm)
 */
package org.jboss.pool;

/**
 *  A listener for object pool events.
 *
 * @author     Aaron Mulder (ammulder@alumni.princeton.edu)
 * @created    August 19, 2001
 */
public interface PoolEventListener {
   /**
    *  The pooled object was closed and should be returned to the pool.
    *
    * @param  evt  Description of Parameter
    */
   public void objectClosed( PoolEvent evt );

   /**
    *  The pooled object had an error and should be returned to the pool.
    *
    * @param  evt  Description of Parameter
    */
   public void objectError( PoolEvent evt );

   /**
    *  The pooled object was used and its timestamp should be updated.
    *
    * @param  evt  Description of Parameter
    */
   public void objectUsed( PoolEvent evt );
}
