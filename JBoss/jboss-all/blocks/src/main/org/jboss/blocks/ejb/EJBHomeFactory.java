/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.blocks.ejb;

import java.util.Map;
import java.util.HashMap;
import java.util.Hashtable;

import javax.ejb.EJBHome;

import javax.naming.InitialContext;
import javax.naming.NamingException;
                    
import javax.rmi.PortableRemoteObject;

/**
 * An <em>EJBHome</em> factory.  Abstracts <tt>EJBHome</tt> lookup 
 * and provides an instance cache to avoid JNDI overhead.
 *
 * <h3>Concurrency</h3>
 * 
 * <p>
 * This class is <b>not</b> thread-safe.
 * Use {@link #makeSynchronized} to obtain a thread-safe object.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class EJBHomeFactory
{
   /**
    * The cached <tt>EJBHome</tt> objects.
    *
    * @todo Use a CachedMap backed by a HashMap so that the VM can reclaim
    *       these references when it needs memory.
    */
   protected final Map cache = new HashMap();
   
   /**
    * The enviroment to use when constructing an <tt>InitialContext</tt>,
    * or null if a plain context should be created.
    */
   protected final Hashtable environment;

   /**
    * An optional prefix which will be prepended to JNDI names for
    * EJBHome lookup.
    */
   protected final String prefix;
   
   /**
    * Construct a <tt>EJBHomeFactory</tt> using the given
    * enviroment properties to construct an <tt>InitialContext</tt> and
    * a JNDI prefix.
    *
    * @param environment    The environment properties used to create
    *                       an <tt>InitialContext</tt>.
    * @param prefix         A JNDI prefix to prepend to JNDI names.
    */
   public EJBHomeFactory(final Hashtable environment, final String prefix)
   {
      this.environment = environment;
      this.prefix = prefix;
   }

   /**
    * Construct a <tt>EJBHomeFactory</tt> using the given
    * enviroment properties to construct an <tt>InitialContext</tt>.
    *
    * @param environment    The environment properties used to create
    *                       an <tt>InitialContext</tt>.
    */
   public EJBHomeFactory(final Hashtable environment)
   {
      this(environment, null);
   }

   /**
    * Construct a <tt>EJBHomeFactory</tt> using a plain
    * <tt>InitialContext</tt>.
    */
   public EJBHomeFactory()
   {
      // use a plain InitialContext
      this(null, null);
   }

   /**
    * Return the JNDI prefix, or null if not used.
    *
    * @return  The JNDI prefix, or null if not used.
    */
   public String getPrefix()
   {
      return prefix;
   }
   
   /**
    * Create a new <tt>InitialContext</tt> object for JNDI lookups.
    *
    * @return    A new <tt>InitialContext</tt> object for JNDI lookups.
    *
    * @throws NamingException    Failed to construct context.
    */
   protected InitialContext createInitialContext()
      throws NamingException
   {
      if (environment != null) {
         return new InitialContext(environment);
      }
      else {
         return new InitialContext();
      }
   }
   
   /**
    * Lookup an <tt>EJBHome</tt> object from JNDI.
    *
    * @param name    The JNDI name of the home object to lookup.
    * @return        The looked home object.
    *
    * @throws NamingException       Failed to lookup home object from JNDI.
    * @throws ClassCastException    The object looked up does not implement
    *                               <tt>EJBHome</tt>.
    */
   protected EJBHome lookupHome(final String name)
      throws NamingException
   {
      InitialContext ctx = createInitialContext();
      try {
         return (EJBHome)ctx.lookup(name);
      }
      finally {
         ctx.close();
      }
   }
   
   /**
    * Lookup an <tt>EJBHome</tt> object and return it.
    *
    * <p>Returns a cached copy of the home object if there is one,
    *    else a new home object will be looked up from JNDI and added
    *    to the cache.
    *
    * @param name    The JNDI name of the home object to lookup.
    * @return        The home object.
    *
    * @throws NamingException       Failed to lookup home object from JNDI.
    */
   public EJBHome create(String name)
      throws NamingException
   {
      if (prefix != null) name = prefix + name;

      EJBHome home = (EJBHome)cache.get(name);

      // if the home object is not in cache, then look it up
      if (home == null) {
         home = lookupHome(name);

         // add the home object to the cache
         cache.put(name, home);
      }

      return home;
   }
   
   /**
    * Lookup an <tt>EJBHome</tt> object and return it.
    *
    * <p>Uses the same semantics as {@link #create(String)} and also
    *    performs {@link PortableRemoteObject#narrow} on the object.
    *
    * @see #create(String)
    * 
    * @param name    The JNDI name of the home object to lookup.
    * @param type    The home interface of the target home object that is
    *                being looked up.
    * @return        The narrowed home object.
    *
    * @throws NamingException       Failed to lookup home object from JNDI.
    * @throws ClassCastException    The narrowed object does not implement
    *                               <tt>EJBHome</tt>.
    */
   public EJBHome create(final String name, final Class type)
      throws NamingException
   {
      // lookup the home object and narrow the interface
      EJBHome home = create(name);
      return (EJBHome)PortableRemoteObject.narrow(home, type);
   }

   /**
    * Lookup an <tt>EJBHome</tt> object and return it.
    *
    * @see #create(String,Class)
    *
    * @param type    The interface of the home object to lookup.
    * @return        The home object.
    *
    * @throws NamingException       Failed to lookup home object from JNDI.
    */
   public EJBHome create(final Class type)
      throws NamingException
   {
      return create(type.getName(), type);
   }

   /**
    * Invalidate the cached home for the given name, if there is one.
    *
    * @param name    The JNDI name of the home object to lookup.
    */
   public void invalidate(String name)
   {
      if (prefix != null) name = prefix + name;

      cache.remove(name);
   }

   /**
    * Invalidate the cached home for the given name, if there is one.
    *
    * @param type    The interface of the home object to invalidate.
    */
   public void invalidate(final Class type)
   {
      invalidate(type.getName());
   }
   
   /**
    * Invalidate all cached home refernces.
    */
   public void invalidate()
   {
      cache.clear();
   }

   
   //////////////////////////////////////////////////////////////////////////
   //                           Singleton Access                           //
   //////////////////////////////////////////////////////////////////////////
   
   /** The global ejb locator instance. */
   private static EJBHomeFactory instance = null;
   
   /**
    * Returns the global instance of the EJBHome factory.
    *
    * <p>The instance will be lazily initialized on the first invocation.
    *    Since many different threads may be accessing this object at same
    *    time, a synchronized factory is created.
    *
    * @return    The global instance of the EJBHome factory.
    */
   public static synchronized EJBHomeFactory getInstance()
   {
      if (instance == null) {
         instance = makeSynchronized(new EJBHomeFactory());
      }
      
      return instance;
   }


   //////////////////////////////////////////////////////////////////////////
   //                           Synchronization                            //
   //////////////////////////////////////////////////////////////////////////
   
   /**
    * Make a synchronized <tt>EJBHomeFactory</tt>.
    *
    * @param aLocator    The EJB locator to make synchronized.
    * @return            A synchronized EJBHome factory..
    */
   public static EJBHomeFactory makeSynchronized(final EJBHomeFactory aFactory)
   {
      return new EJBHomeFactory()
         {
            private final EJBHomeFactory factory = aFactory;

            public synchronized String getPrefix()
            {
               return factory.getPrefix();
            }
            
            public synchronized EJBHome create(final String name)
               throws NamingException
            {
               return factory.create(name);
            }

            public synchronized EJBHome create(final String name, final Class type)
               throws NamingException
            {
               return factory.create(name, type);
            }
            
            public synchronized EJBHome create(final Class type)
               throws NamingException
            {
               return factory.create(type);
            }

            public synchronized void invalidate(final String name)
            {
               factory.invalidate(name);
            }

            public synchronized void invalidate(final Class type)
            {
               factory.invalidate(type);
            }
   
            public synchronized void invalidate()
            {
               factory.invalidate();
            }
         };
   }
}
