/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.coerce;

import java.util.Map;
import java.util.HashMap;
import java.util.Collections;

import org.jboss.util.CoercionException;
import org.jboss.util.NotCoercibleException;
import org.jboss.util.NullArgumentException;
import org.jboss.util.NotImplementedException;

/**
 * An abstract class to allow extending the default behavior of
 * {@link org.jboss.util.Objects#coerce(Object,Class)} when it is 
 * not possible to implement {@link org.jboss.util.Coercible} 
 * directly in the target class or where coercion is desired from 
 * an unrelated class.  Also provides a registry for all of the 
 * currently installed handlers.
 *
 * @todo Implement a URL package handler style method for finding
 *       handlers as well as the current explict methods.
 * @todo Add URL handler.
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public abstract class CoercionHandler
{
   /**
    * Coerce the given value into the specified type.
    *
    * @param value      Value to coerce
    * @param type       Object type to coerce into
    * @return           Coerced value
    *
    * @exception CoercionException        Failed to coerce
    */
   public abstract Object coerce(Object value, Class type)
      throws CoercionException;

   /**
    * Get the target class type for this CoercionHandler.
    *
    * @return     Class type
    *
    * @throws NotImplementedException  Handler is not bound
    */
   public Class getType() {
      throw new NotImplementedException("handler is not bound");
   }

   /////////////////////////////////////////////////////////////////////////
   //                             Factory Methods                         //
   /////////////////////////////////////////////////////////////////////////

   /** Class -> CoercionHandler map */
   private static Map handlers = Collections.synchronizedMap(new HashMap());

   /** Initializes the CoercionHandler map */
   static {
      // initialzie the helper map with some defaults
      install(new CharacterHandler());
      install(new ClassHandler());
      install(new FileHandler());
   }

   /**
    * Install a CoercionHandler for a given class type.
    *
    * @param handler    Coercion handler
    *
    * @throws NullArgumentException    type or handler
    */
   public static void install(Class type, CoercionHandler handler) {
      if (type == null)
         throw new NullArgumentException("type");
      if (handler == null)
         throw new NullArgumentException("handler");

      handlers.put(type, handler);
   }

   /**
    * Install a BoundCoercionHandler.
    *
    * @param handler    Bound coercion handler
    *
    * @throws NullArgumentException    handler
    */
   public static void install(BoundCoercionHandler handler) {
      if (handler == null)
         throw new NullArgumentException("handler");

      handlers.put(handler.getType(), handler);
   }

   /**
    * Uninstall a CoercionHandler for a given class type.
    *
    * @param type    Class type
    *
    * @throws NullArgumentException    type
    */
   public static void uninstall(Class type) {
      if (type == null)
         throw new NullArgumentException("type");

      handlers.remove(type);
   }

   /**
    * Check if there is a CoercionHandler installed for the given class.
    *
    * @param type    Class type
    * @return        True if there is a CoercionHandler
    */
   public static boolean isInstalled(Class type) {
      return handlers.containsKey(type);
   }

   /**
    * Lookup the CoercionHandler for a given class.
    *
    * @param type    Class type
    * @return        A CoercionHandler or null if there is no installed handler
    *
    * @throws NullArgumentException    type
    */
   public static CoercionHandler lookup(Class type) {
      if (type == null)
         throw new NullArgumentException("type");

      return (CoercionHandler)handlers.get(type);
   }

   /**
    * Create a CoercionHandler for the given class type.
    *
    * @param type    Class type
    * @return        A CoercionHandler instance for the given class type.
    *
    * @throws CoercionException  No installed handler for type
    */
   public static CoercionHandler create(Class type) {
      CoercionHandler handler = lookup(type);
      if (handler == null)
         throw new CoercionException
            ("no installed handler for type: " + type);

      return handler;
   }
}
