/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.invocation;

import java.io.InputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectStreamClass;
import java.lang.reflect.Proxy;
import org.jboss.util.collection.WeakValueHashMap;

import org.jboss.logging.Logger;

/**
 * An ObjectInputStream subclass used by the MarshalledValue class to
 * ensure the classes and proxies are loaded using the thread context
 * class loader.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.5 $
 */
public class MarshalledValueInputStream
   extends ObjectInputStream
{
   private static Logger log = Logger.getLogger(MarshalledValueInputStream.class);
   /** A class wide cache of proxy classes populated by resolveProxyClass */
   private static WeakValueHashMap classCache;

   /** Enable local caching of resolved proxy classes. This can only be used
    * if there is a single ULR and no redeployment of the proxy classes.
    *
    * @param flag true to enable caching, false to disable it
    */
   public static void useClassCache(boolean flag)
   {
      if( flag == true )
         classCache = new WeakValueHashMap();
      else
         classCache = null;
   }

   /** Clear the current proxy cache.
    *
    */
   public static void flushClassCache()
   {
      classCache.clear();
   }

   /**
    * Creates a new instance of MarshalledValueOutputStream
    */
   public MarshalledValueInputStream(InputStream is) throws IOException
   {
      super(is);
   }

   /**
    * Use the thread context class loader to resolve the class
    *
    * @throws IOException   Any exception thrown by the underlying OutputStream.
    */
   protected Class resolveClass(ObjectStreamClass v)
      throws IOException, ClassNotFoundException
   {
      String className = v.getName();
      Class resolvedClass = null;
      // Check the class cache first if it exists
      if( classCache != null )
      {
         synchronized( classCache )
         {
            resolvedClass = (Class) classCache.get(className);
         }
      }

      if( resolvedClass == null )
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         try
         {
            resolvedClass = loader.loadClass(className);
         }
         catch(ClassNotFoundException e)
         {
            /* Use the Class.forName call which will resolve array classes. We
            do not use this by default as this can result in caching of stale
            values across redeployments.
            */
            resolvedClass = Class.forName(className, false, loader);
         }
         if( classCache != null )
         {
            synchronized( classCache )
            {
               classCache.put(className, resolvedClass);
            }
         }
      }
      return resolvedClass;
   }

   protected Class resolveProxyClass(String[] interfaces)
      throws IOException, ClassNotFoundException
   {
      if( log.isTraceEnabled() )
      {
         StringBuffer tmp = new StringBuffer("[");
         for(int i = 0; i < interfaces.length; i ++)
         {
            if( i > 0 )
               tmp.append(',');
            tmp.append(interfaces[i]);
         }
         tmp.append(']');
         log.trace("resolveProxyClass called, ifaces="+tmp.toString());
      }

      // Load the interfaces from the cache or thread context class loader
      ClassLoader loader = null;
      Class[] ifaceClasses = new Class[interfaces.length];
      for (int i = 0; i < interfaces.length; i++)
      {
         Class iface = null;
         String className = interfaces[i];
         // Check the proxy cache if it exists
         if( classCache != null )
         {
            synchronized( classCache )
            {
               iface = (Class) classCache.get(className);
            }
         }

         // Load the interface class using the thread context ClassLoader
         if( iface == null )
         {
            if( loader == null )
               loader = Thread.currentThread().getContextClassLoader();
            iface = loader.loadClass(className);
            if( classCache != null )
            {
               synchronized( classCache )
               {
                  classCache.put(className, iface);
               }
            }
         }
         ifaceClasses[i] = iface;
      }

      return Proxy.getProxyClass(loader, ifaceClasses);
   }
}
