/*
 * Licensed under the X license (see http://www.x.org/terms.htm)
 */
package org.jboss.pool;

import java.util.EventObject;

/**
 *  An event caused by an object in a pool. The event indicates that the object
 *  was used, closed, or had an error occur. The typical response is to update
 *  the last used time in the pool for used events, and return the object to the
 *  pool for closed or error events.
 *
 * @author     Aaron Mulder (ammulder@alumni.princeton.edu)
 * @created    August 19, 2001
 */
public class PoolEvent extends EventObject {

   private int      type;
   private boolean  catastrophic = false;
   /**
    *  The object has been closed and should be returned to the pool. Note this
    *  is not a final sort of closing - the object must still be able to be
    *  returned to the pool and reused.
    */
   public final static int OBJECT_CLOSED = -8986432;
   /**
    *  Indicates that an error occured with the object. The object will be
    *  returned to the pool, since there will presumably be an exception thrown
    *  that precludes the client from closing it or returning it normally. This
    *  should not be used for final or destructive errors - the object must stil
    *  be able to be returned to the pool and reused.
    */
   public final static int OBJECT_ERROR = -8986433;
   /**
    *  Indicates that the object was used, and its timestamp should be updated
    *  accordingly (if the pool tracks timestamps).
    */
   public final static int OBJECT_USED = -8986434;

   /**
    *  Create a new event.
    *
    * @param  source  The source must be the object that was returned from the
    *      getObject method of the pool - the pool will use the source for some
    *      purpose depending on the type, so it cannot be an arbitrary object.
    * @param  type    The event type.
    */
   public PoolEvent( Object source, int type ) {
      super( source );
      if ( type != OBJECT_CLOSED && type != OBJECT_ERROR && type != OBJECT_USED ) {
         throw new IllegalArgumentException( "Invalid event type!" );
      }
      this.type = type;
   }

   /**
    *  Marks this as an error so severe that the object should not be reused by
    *  the pool.
    */
   public void setCatastrophic() {
      catastrophic = true;
   }

   /**
    *  Gets the event type.
    *
    * @return    The Type value
    * @see       #OBJECT_CLOSED
    * @see       #OBJECT_USED
    * @see       #OBJECT_ERROR
    */
   public int getType() {
      return type;
   }

   /**
    *  Gets whether an object error was so bad that the object should not be
    *  reused by the pool. This is meaningful for error events only.
    *
    * @return    The Catastrophic value
    */
   public boolean isCatastrophic() {
      return catastrophic;
   }
}
