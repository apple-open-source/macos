/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.util;


/**
 * A synchronized cache policy wrapper.
 *
 * @author <a href="mailto:julien@jboss.org">Julien Viet</a>
 * @version $Revision: 1.1.2.1 $
 * @see CachePolicy
 */
public final class SynchronizedCachePolicy
   implements CachePolicy
{

   // Attributes ----------------------------------------------------

   private final CachePolicy delegate;

   // Constructors --------------------------------------------------

   public SynchronizedCachePolicy(CachePolicy delegate)
   {
      this.delegate = delegate;
   }

   // CachePolicy implementation ------------------------------------

   synchronized public Object get(Object key)
   {
      return delegate.get(key);
   }

   synchronized public Object peek(Object key)
   {
      return delegate.get(key);
   }

   synchronized public void insert(Object key, Object object)
   {
      delegate.insert(key, object);
   }

   synchronized public void remove(Object key)
   {
      delegate.remove(key);
   }

   synchronized public void flush()
   {
      delegate.flush();
   }

   synchronized public int size()
   {
      return delegate.size();
   }

   synchronized public void create() throws Exception
   {
      delegate.create();
   }

   synchronized public void start() throws Exception
   {
      delegate.start();
   }

   synchronized public void stop()
   {
      delegate.stop();
   }

   synchronized public void destroy()
   {
      delegate.destroy();
   }
}
