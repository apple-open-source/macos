/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.invocation.pooled.interfaces;

import java.io.InputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectStreamClass;
import java.lang.reflect.Proxy;
import java.lang.reflect.Method;
import java.lang.ref.WeakReference;

import org.jboss.util.collection.WeakValueHashMap;

import org.jboss.logging.Logger;
import org.jboss.invocation.MarshalledValueInputStream;
import EDU.oswego.cs.dl.util.concurrent.ConcurrentReaderHashMap;

/**
 * An ObjectInputStream subclass used by the MarshalledValue class to
 * ensure the classes and proxies are loaded using the thread context
 * class loader.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class OptimizedObjectInputStream
        extends ObjectInputStream
{
   private static Logger log = Logger.getLogger(OptimizedObjectInputStream.class);
   /** A class wide cache of proxy classes populated by resolveProxyClass */
   private static ConcurrentReaderHashMap classCache;
   private static ConcurrentReaderHashMap objectStreamClassCache;
   private static Method lookupStreamClass = null;

   static
   {
      useClassCache(true);
      try
      {
         lookupStreamClass = ObjectStreamClass.class.getDeclaredMethod("lookup", new Class[]{Class.class, boolean.class});
         lookupStreamClass.setAccessible(true);
      }
      catch (Exception ex)
      {
         ex.printStackTrace();
      }
   }

   /** Enable local caching of resolved proxy classes. This can only be used
    * if there is a single ULR and no redeployment of the proxy classes.
    *
    * @param flag true to enable caching, false to disable it
    */
   public static void useClassCache(boolean flag)
   {
      if (flag == true)
      {
         classCache = new ConcurrentReaderHashMap();
         objectStreamClassCache = new ConcurrentReaderHashMap();
      }
      else
      {
         classCache = null;
         objectStreamClassCache = null;
      }
   }

   /** Clear the current proxy cache.
    *
    */
   public static void flushClassCache()
   {
      classCache.clear();
      objectStreamClassCache.clear();
   }

   private static Class forName(String className) throws ClassNotFoundException
   {
      Class clazz = null;
      if (classCache != null)
      {
         WeakReference ref = (WeakReference) classCache.get(className);
         if (ref != null) clazz = (Class) ref.get();
         if (clazz == null)
         {
            if (ref != null) classCache.remove(className);
            ClassLoader loader = Thread.currentThread().getContextClassLoader();
            try
            {
               clazz = loader.loadClass(className);
            }
            catch (ClassNotFoundException e)
            {
               /* Use the Class.forName call which will resolve array classes. We
               do not use this by default as this can result in caching of stale
               values across redeployments.
               */
               clazz = Class.forName(className, false, loader);
            }
            classCache.put(className, new WeakReference(clazz));
         }
      }
      else
      {
         clazz = Thread.currentThread().getContextClassLoader().loadClass(className);
      }
      return clazz;
   }

   /**
    * Creates a new instance of MarshalledValueOutputStream
    */
   public OptimizedObjectInputStream(InputStream is) throws IOException
   {
      super(is);
   }

   protected static ObjectStreamClass lookup(Class clazz)
   {
      Object[] args = {clazz, Boolean.TRUE};
      try
      {
         return (ObjectStreamClass) lookupStreamClass.invoke(null, args);
      }
      catch (Exception ex)
      {
         ex.printStackTrace();
      }
      return null;
   }

   protected ObjectStreamClass readClassDescriptor()
           throws IOException, ClassNotFoundException
   {
      String className = readUTF();
      ObjectStreamClass osc = null;
      if (objectStreamClassCache != null)
      {
         osc = (ObjectStreamClass) objectStreamClassCache.get(className);
      }
      if (osc == null)
      {
         Class clazz = forName(className);
         osc = ObjectStreamClass.lookup(clazz);
         if (osc == null) osc = lookup(clazz);
         if (osc == null) throw new IOException("Unable to readClassDescriptor for class " + className);
         if (objectStreamClassCache != null) objectStreamClassCache.put(className, osc);
      }
      return osc;
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
      return forName(className);
   }

   protected Class resolveProxyClass(String[] interfaces)
           throws IOException, ClassNotFoundException
   {
      // Load the interfaces from the cache or thread context class loader
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      Class[] ifaceClasses = new Class[interfaces.length];
      for (int i = 0; i < interfaces.length; i++)
      {
         String className = interfaces[i];
         Class iface = forName(className);
         ifaceClasses[i] = iface;
      }

      return Proxy.getProxyClass(loader, ifaceClasses);
   }
}
