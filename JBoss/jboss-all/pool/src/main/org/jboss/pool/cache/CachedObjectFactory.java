/*
 * Licensed under the X license (see http://www.x.org/terms.htm)
 */
package org.jboss.pool.cache;

/**
 *  Creates objects for a cache. The cache is essentially a map, so this factory
 *  is given a "key" and creates the appropriate "value" which will be cached.
 *  There are a number of other functions that can be overridden for more
 *  specific control, but createObject is the only one that's required.
 *
 * @author     Aaron Mulder ammulder@alumni.princeton.edu
 * @created    August 18, 2001
 */
public abstract class CachedObjectFactory {

   public CachedObjectFactory() {
   }

   /**
    *  Creates a new object to be stored in an object cache. This is the
    *  instance that will actually be stored in the cache and reused. If you
    *  want to wrap it somehow, or return instances of a different type that
    *  refers to these, you can implement prepareObject.
    *
    * @param  identifier  Description of Parameter
    * @return             Description of the Returned Value
    * @see                #prepareObject
    */
   public abstract Object createObject( Object identifier );

   /*
    * Indicates to the factory that the cache has started up.  This will be
    * called before any other methods of the factory are called (on behalf of
    * this cache).
    * @param cache The cache that is starting.  You may decide to allow
    * multiple cached you use your factory, or to restrict it to a one-to-one
    * relationship.
    * @throws java.lang.IllegalArgumentException
    * Occurs when the cache is null.
    */
   public void cacheStarted( ObjectCache cache ) {
      if ( cache == null ) {
         throw new IllegalArgumentException( "Cannot start factory with null cache!" );
      }
   }

   /**
    *  Prepares an object to be returned to the client. This may be used to
    *  configure the object somehow, or actually return a completely different
    *  object (so long as the original can be recovered in translateObject. This
    *  will be called whenever an object is returned to the client, whether a
    *  new object or a cached object.
    *
    * @param  cachedObject  The object in the cache, as created by createObject.
    * @return               The object to return to the client. If different,
    *      the cached object must be recoverable by translateObject.
    */
   public Object prepareObject( Object cachedObject ) {
      return cachedObject;
   }

   /**
    *  If the objects supplied to the client are different than the objects in
    *  the cache, extracts a cache object from a client object. This may be
    *  called once after an object has been released if the garbage collector
    *  and a client attempt to release an object at the same time. In this case,
    *  this method may work, return null, or throw an exception and the cache
    *  will handle it gracefully. The default implementation returns the
    *  parameter object (assumes client and cached objects are the same).
    *
    * @param  clientObject  The client object, as returned by prepareObject
    * @return               The cached object, as originally returned by
    *      createObject
    */
   public Object translateObject( Object clientObject ) {
      return clientObject;
   }

   /**
    *  Indicates to the factory that the cache is closing down. This will be
    *  called before all the instances are destroyed. There may be calls to
    *  returnObject or translateObject after this, but no calls to createObject
    *  or prepareObject (on behalf of this cache).
    *
    * @param  cache                                The cache that is closing.
    *      You may decide to allow multiple caches you use your factory, or to
    *      restrict it to a one-to-one relationship.
    * @throws  java.lang.IllegalArgumentException  Occurs when the pool is null.
    */
   public void cacheClosing( ObjectCache cache ) {
      if ( cache == null ) {
         throw new IllegalArgumentException( "Cannot close factory with a null cache!" );
      }
   }

   /**
    *  Permanently closes an object, after it is removed from the cache. The
    *  object will not be returned to the cache - after this, it is gone. This
    *  is called when the cache is full and new objects are added, and when the
    *  cache is closed.
    *
    * @param  pooledObject  Description of Parameter
    */
   public void deleteObject( Object pooledObject ) {
   }
}
