/*
 * Licensed under the X license (see http://www.x.org/terms.htm)
 */
package org.jboss.pool;

import java.util.ConcurrentModificationException;
import java.util.Date;
import java.util.EventObject;

/**
 *  Stores the properties of an object in a pool.
 *
 * @author     Aaron Mulder (ammulder@alumni.princeton.edu)
 * @created    August 18, 2001
 */
class ObjectRecord {
   private long     created;
   private long     lastUsed;
   private Object   object;
   private Object   clientObject;
   private boolean  inUse;

   /**
    *  Created a new record for the specified pooled object. Objects default to
    *  being in use when created, so that they can't be stolen away from the
    *  creator by another thread.
    *
    * @param  ob  Description of Parameter
    */
   public ObjectRecord( Object ob ) {
      this( ob, true );
   }

   /**
    *  Created a new record for the specified pooled object. Sets the initial
    *  state to in use or not.
    *
    * @param  ob     Description of Parameter
    * @param  inUse  Description of Parameter
    */
   public ObjectRecord( Object ob, boolean inUse ) {
      created = lastUsed = System.currentTimeMillis();
      object = ob;
      this.inUse = inUse;
   }

   /**
    *  Sets whether this connection is currently in use.
    *
    * @param  inUse                                       The new InUse value
    * @exception  ConcurrentModificationException         Description of
    *      Exception
    * @throws  java.util.ConcurrentModificationException  Occurs when the
    *      connection is already in use and it is set to be in use, or it is not
    *      in use and it is set to be not in use.
    */
   public synchronized void setInUse( boolean inUse )
      throws ConcurrentModificationException {
      if ( this.inUse == inUse ) {
         throw new ConcurrentModificationException();
      }
      this.inUse = inUse;
      lastUsed = System.currentTimeMillis();
      if ( !inUse ) {
         clientObject = null;
      }
   }

   /**
    *  Sets the last used time to the current time.
    */
   public void setLastUsed() {
      lastUsed = System.currentTimeMillis();
   }

   /**
    *  Sets the client object associated with this object. Not always used.
    *
    * @param  o  The new ClientObject value
    */
   public void setClientObject( Object o ) {
      clientObject = o;
   }

   /**
    *  Gets the date when this connection was originally opened.
    *
    * @return    The CreationDate value
    */
   public Date getCreationDate() {
      return new Date( created );
   }

   /**
    *  Gets the date when this connection was last used.
    *
    * @return    The LastUsedDate value
    */
   public Date getLastUsedDate() {
      return new Date( lastUsed );
   }

   /**
    *  Gets the time (in milliseconds) since this connection was last used.
    *
    * @return    The MillisSinceLastUse value
    */
   public long getMillisSinceLastUse() {
      return System.currentTimeMillis() - lastUsed;
   }

   /**
    *  Tells whether this connection is currently in use. This is not
    *  synchronized since you probably want to synchronize at a higher level (if
    *  not in use, do something), etc.
    *
    * @return    The InUse value
    */
   public boolean isInUse() {
      return inUse;
   }

   /**
    *  Gets the pooled object associated with this record.
    *
    * @return    The Object value
    */
   public Object getObject() {
      return object;
   }

   /**
    *  Gets the client object associated with this object. If there is none,
    *  returns the normal object (which is the default).
    *
    * @return    The ClientObject value
    */
   public Object getClientObject() {
      return clientObject == null ? object : clientObject;
   }

   /**
    *  Shuts down this object - it will be useless thereafter.
    */
   public void close() {
      object = null;
      clientObject = null;
      created = lastUsed = Long.MAX_VALUE;
      inUse = true;
   }
}
