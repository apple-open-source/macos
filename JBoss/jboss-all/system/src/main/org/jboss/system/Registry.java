/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import java.util.Map;

import org.jboss.logging.Logger;

import EDU.oswego.cs.dl.util.concurrent.ConcurrentReaderHashMap;

/**
 * A registry, really, a registry.
 *
 * <p>All methods static to lookup pointers from anyplace in the VM.  
 *    We use it for hooking up JMX managed objects.  Use the JMX MBeanName 
 *    to put objects here.
 *  
 * @author <a href="mailto:marc.fleury@jboss.org>Marc Fleury</a>
 * @version $Revision: 1.2.2.4 $
 */
public class Registry
{
   private static final Logger log = Logger.getLogger(Registry.class);
   
   public static Map entries = new ConcurrentReaderHashMap();
   
   public static void bind(final Object key, final Object value)
   {
      entries.put(key, value);
      if(log.isTraceEnabled())
         log.trace("bound " + key + "=" + value);
   }
   
   public static Object unbind(final Object key)
   {
      Object obj = entries.remove(key);
      if(log.isTraceEnabled())
         log.trace("unbound " + key + "=" + obj);
      return obj;
   }
   
   public static Object lookup(final Object key)
   {
      Object obj = entries.get(key);
      if(log.isTraceEnabled())
         log.trace("lookup " + key + "=" + obj);
      return obj;
   }
}
