/*
 * Licensed under the X license (see http://www.x.org/terms.htm)
 */
package org.jboss.pool;

/**
 *  Optional interface for an object in an ObjectPool. If the objects created by
 *  the ObjcetFactory implement this, the pool will register as a listener when
 *  an object is checked out, and deregister when the object is returned. Then
 *  if the object sends a close or error event, the pool will return the object
 *  to the pool without the client having to do so explicitly.
 *
 * @author     Aaron Mulder (ammulder@alumni.princeton.edu)
 * @created    August 19, 2001
 */
public interface PooledObject {
   /**
    *  Adds a new listener.
    *
    * @param  listener  The feature to be added to the PoolEventListener
    *      attribute
    */
   public void addPoolEventListener( PoolEventListener listener );

   /**
    *  Removes a listener.
    *
    * @param  listener  Description of Parameter
    */
   public void removePoolEventListener( PoolEventListener listener );
}
