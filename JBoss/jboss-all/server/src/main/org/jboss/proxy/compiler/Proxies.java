/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy.compiler;

import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Modifier;
import java.lang.reflect.Member;

import java.io.Serializable;

import java.util.ArrayList;
import java.util.Hashtable;

/**
 * Routines for converting between strongly-typed interfaces and
 * generic InvocationHandler objects.
 *
 * @version <tt>$Revision: 1.4.2.1 $</tt>
 * @author Unknown
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class Proxies
{
   /**
    * Disallow creation of Proxyies instances.
    */
   private Proxies()
   {
      super();
   }
   
   /**
    * Create a new target object <em>x</em> which is a proxy for
    * the given InvocationHandler <tt>disp</tt>.  The new object will be
    * equivalent to <tt>disp</tt>, except that it will support normal Java
    * method invocation, in place of the <tt>InvocationHandler.invoke</tt>
    * protocol, for each method supported by the InvocationHandler.
    *
    * <p>
    * The new object will implement each of the given target types.
    * (The target types default to those declared by the InvocationHandler
    * itself.) The new object will also implement the "administrative"
    * interface <tt>Proxies.ProxyTarget</tt>.
    *
    * <p>
    * For each "overrideable" (public, non-static, non-final)
    * method <tt>T.m</tt> of <tt>T</tt>, calling <tt>x.m(...)</tt>
    * will immediately cause a corresponding reflective call of the
    * form <tt>disp.invoke(RM, new Object[]{ ... })</tt>, where <tt>RM</tt>
    * is the reflective object for <tt>m</tt>.
    *
    * <p>
    * The concrete class of this target object will be
    * something mysterious and indefinite.  Many callers will
    * immediately cast the resulting proxy object to the target type
    * of the InvocationHandler.  For example:
    * <code>
    * MyInterface x1 = ...;
    * InvocationHandler i = Proxies.newInvocationHandler(x1, MyInterface.class);
    * MyInterface x2 = (MyInterface) ((Proxies.ProxyInvocationHandler)i).getTarget();
    *   // x1 == x2
    * MyInterface x3 = (MyInterface) Proxies.newTarget(i);
    *   // x1 != x3, but calls to x3 are forwarded via i to x1
    * </code>    
    */
   public static ProxyTarget newTarget(ClassLoader parent,
                                       InvocationHandler invocationHandler,
                                       Class targetTypes[])
      throws Exception
   {
      return Impl.getImpl(targetTypes).newTarget(invocationHandler, parent);
   }
   
   /**
    * A common interface shared by all objects created
    * by <tt>Proxies.newTarget</tt>.
    */
   public interface ProxyTarget
      extends Serializable
   {
      /**
       * Recover the original InvocationHandler object around which this
       * proxy is wrapped.
       */
      InvocationHandler getInvocationHandler();
      
      /**
       * Recover the original target types for which this proxy was wrapped.
       */
      Class[] getTargetTypes();
   }
   
   /**
    * Create a new reflective InvocationHandler object
    * <tt>InvocationHandler</tt> wrapped around the given target object, for
    * the given target type(s).
    *
    * <p>
    * The new object will be operationally equivalent to <tt>target</tt>,
    * except that it will support a reflective method invocation sequence
    * (<tt>InvocationHandler.invoke</tt>) instead of the normal Java method
    * invocation mechanism.
    *
    * <p>
    * The target type must be specified, since the complete implementation
    * type of the target object is usually irrelevant to the application.
    * The target object must match the specified target type.
    * For example:
    * <code>
    * MyInterface x1 = ...;
    * InvocationHandler i = Proxies.newInvocationHandler(x1, MyInterface.class);
    * </code>
    */
   public static ProxyInvocationHandler newInvocationHandler(Object target,
                                                             Class targetType)
   {
      return Impl.getImpl(targetType).newInvocationHandler(target);
   }
   
   public static ProxyInvocationHandler newInvocationHandler(Object target,
                                                             Class targetTypes[])
   {
      return Impl.getImpl(targetTypes).newInvocationHandler(target);
   }
   
   /**
    * A common interface shared by all objects created
    * by <tt>Proxies.newInvocationHandler</tt>.
    */
   public interface ProxyInvocationHandler
      extends InvocationHandler, Serializable
   {
      /**
       * Recover the original target object around which this
       * InvocationHandler proxy is wrapped.
       */
      Object getTarget();
   }
   
   /**
    * Utility built on top of <tt>newTarget</tt> to find
    * or create a proxy for the given InvocationHandler.
    * It is the inverse of <tt>getInvocationHandler</tt>.
    *
    * <p>
    * If the InvocationHandler implements <tt>ProxyInvocationHandler</tt>,
    * it is a proxy for some original target object; extract and return
    * that object.  Otherwise, just call <tt>newTarget</tt>.
    */
   public static Object getTarget(InvocationHandler invocationHandler)
   {
      if (invocationHandler instanceof ProxyInvocationHandler)
      {
         Object target = ((ProxyInvocationHandler)invocationHandler).getTarget();
         if (target != null)
         {
            return target;
         }
         // and fall through...
      }
      
      return null;
   }
   
   /**
    * Utility built on top of <tt>newInvocationHandler</tt> to find
    * or create a proxy for the given target object.
    * It is the inverse of <tt>getTarget</tt>.
    *
    * <p>
    * If the target implements <tt>ProxyTarget</tt>, it is a proxy
    * for some original InvocationHandler; extract and return that
    * InvocationHandler.  Otherwise, just call <tt>newInvocationHandler</tt>.
    *
    * @see #newInvocationHandler
    */
   public static InvocationHandler getInvocationHandler(Object target,
                                                        Class targetTypes[])
   {
      if (target instanceof ProxyTarget)
      {
         ProxyTarget tproxy = (ProxyTarget)target;
         InvocationHandler invocationHandler = tproxy.getInvocationHandler();
         if (targetTypes == null ||
         Impl.sameTypes(tproxy.getTargetTypes(), targetTypes))
         {
            return invocationHandler;
         }
         // and fall through...
      }
      return newInvocationHandler(target, targetTypes);
   }
   
   public static InvocationHandler getInvocationHandler(Object target,
                                                        Class targetType)
   {
      // (should this be optimized?)
      if (targetType == null)
      {
         return getInvocationHandler(target, (Class[])null);
      }

      return getInvocationHandler(target, new Class[] { targetType });
   }
   
   /**
    * Utility which reports the set of valid methods for a target type.
    * It is exactly the set of <tt>public</tt>, <tt>abstract</tt> methods
    * returned by <tt>targetType.getMethods()</tt>, which are neither
    * <tt>static</tt> nor <tt>final</tt>.
    * <p>
    * Also, if the targetType is not a suitable type, an empty array
    * will be returned.  The target type must not contain <tt>protected</tt>
    * <tt>abstract</tt> methods, must have a nullary constructor,
    * and must not be something silly like
    * an array or primitive type, or a <tt>final</tt> class.
    */
   public static Method[] getMethods(Class targetType)
   {
      return Impl.getImpl(targetType).copyMethods();
   }
   
   public static Method[] getMethods(Class targetTypes[])
   {
      return Impl.getImpl(targetTypes).copyMethods();
   }

   public static void forgetProxyForClass(Class clazz)
   {
      Impl.forgetProxyForClass(clazz);
   }
   
   /**
    * ???
    */
   static class Impl
      implements Serializable
   {
      static Hashtable impls = new Hashtable();
      
      /** the types that this impl processes */
      Class targetTypes[];
      
      Method methods[];
      
      /** hashtable link to Impls sharing a target type */
      Impl more;
      
      Class superclass = Object.class;
      
      /** used in print names of proxies */
      String proxyString;
      
      Constructor proxyConstructor;
      
      Impl(Class targetTypes[])
      {
         this.targetTypes = targetTypes;
         
         Method methodLists[][] = new Method[targetTypes.length][];
         for (int i = 0; i < targetTypes.length; i++)
         {
            methodLists[i] = checkTargetType(targetTypes[i]);
         }

         checkSuperclass();
         this.methods = combineMethodLists(methodLists);
      }
      
      static synchronized Impl getImpl(Class targetType)
      {
         Impl impl = (Impl) impls.get(targetType);
         if (impl == null)
         {
            impl = new Impl(new Class[] { targetType });
            impls.put(targetType, impl);
         }
         return impl;
      }
      
      static synchronized Impl getImpl(Class targetTypes[])
      {
         int n = targetTypes.length;
         if (n == 1)
         {
            return getImpl(targetTypes[0]);
         }
         // note that the desired Impl could be in any one of n places
         // this requires extra searching, which is not a big deal
         for (int i = 0; i < n; ++i)
         {
            for (Impl impl = (Impl) impls.get(targetTypes[i]); impl != null; impl = impl.more)
            {
               if (sameTypes(targetTypes, impl.targetTypes))
                  return impl;
            }
         }
         
         // now link it into the table
         targetTypes = copyAndUniquify(targetTypes);
         Impl impl1 = getImpl(new Class[] { targetTypes[0] });
         Impl impl = new Impl(targetTypes);
         impl.more = impl1.more;
         impl1.more = impl;
         return impl;
      }
      
      /**
       * The <code>forgetProxyForClass</code> method removes the impl from the 
       * class-impl map.  This releases the UnifiedClassloader used to load the 
       * class we are constructing the proxy for.
       *
       * This may not work if the original class[] contained many classes, but
       * seems OK with one class + Serializable, which is what is used by the cmp2
       * engine.  At present the cmp2 engine is the only caller of this method 
       * (through Proxy).
       *
       * @param clazz a <code>Class</code> value
       */
      static synchronized void forgetProxyForClass(Class clazz)
      {
         impls.remove(clazz);
      }


      // do the arrays have the same elements?
      // (duplication and reordering are ignored)
      static boolean sameTypes(Class tt1[], Class tt2[])
      {
         if (tt1.length == 1 && tt2.length == 1)
         {
            return tt1[0] == tt2[0];
         }
         
         int totalSeen2 = 0;
         each_type:
            for (int i = tt1.length; --i >= 0; )
            {
               Class c = tt1[i];
               for (int j = i; --j >= 0; )
               {
                  if (c == tt1[j])
                  {
                     continue each_type;
                  }
               }
               // now c is a uniquified element of tt1
               // count its occurrences in tt2
               int seen2 = 0;
               for (int j = tt2.length; --j >= 0; )
               {
                  if (c == tt2[j])
                  {
                     ++seen2;
                  }
               }

               if (seen2 == 0)
               {
                  // c does not occur in tt2
                  return false;
               }
               totalSeen2 += seen2;
            }
            // now, each element of tt2 must have been visited
            return totalSeen2 == tt2.length;
      }
      
      static Class[] copyAndUniquify(Class targetTypes[])
      {
         int n = targetTypes.length;
         Class tt[] = new Class[n];
         int k = 0;
         each_type:
            for (int i = 0; i < n; i++)
            {
               Class c = targetTypes[i];
               for (int j = i; --j >= 0; )
               {
                  if (c == targetTypes[j])
                  {
                     continue each_type;
                  }
               }
               tt[k++] = c;
            }
            if (k < n)
            {
               // oops; caller passed in duplicate
               Class tt0[] = new Class[k];
               for (int i = 0; i < k; i++)
               {
                  tt0[i] = tt[i];
               }
               tt = tt0;
            }
            return tt;
      }
      
      // make sure a give target type is acceptable
      // return a list of eligible methods (may also include nulls)
      Method[] checkTargetType(Class targetType)
      {
         if (targetType.isArray())
         {
            throw new IllegalArgumentException
               ("cannot subclass an array type: " + targetType.getName());
         }

         if (targetType.isPrimitive())
         {
            throw new IllegalArgumentException
               ("cannot subclass a primitive type: " + targetType);
         }

         int tmod = targetType.getModifiers();
         if (Modifier.isFinal(tmod))
         {
            throw new IllegalArgumentException
               ("cannot subclass a final type: " + targetType);
         }

         if (!Modifier.isPublic(tmod))
         {
            throw new IllegalArgumentException
               ("cannot subclass a non-public type: " + targetType);
         }
         
         // Make sure the subclass will not need a "super" statement.
         if (!targetType.isInterface())
         {
            if (!targetType.isAssignableFrom(superclass))
            {
               if (superclass.isAssignableFrom(targetType))
               {
                  superclass = targetType;
               } 
               else {
                  throw new IllegalArgumentException
                     ("inconsistent superclass: " + targetType);
               }
            }
         }
         
         // Decide what overrideable methods this type supports.
         Method methodList[] = targetType.getMethods();
         int nm = 0;
         for (int i = 0; i < methodList.length; i++)
         {
            Method m = methodList[i];
            if (eligibleForInvocationHandler(m))
            {
               methodList[nm++] = m;    // (reuse the method array)
            }
         }

         while (nm < methodList.length)
         {
            methodList[nm++] = null;     // (pad the reused method array)
         }
         
         return methodList;
      }
      
      void checkSuperclass()
      {
         Constructor constructors[] = superclass.getConstructors();
         for (int i = 0; i < constructors.length; i++)
         {
            Constructor c = constructors[i];
            int mod = c.getModifiers();
            if (Modifier.isPublic(mod)
                && c.getParameterTypes().length == 0)
            {
               return;  // OK
            }
         }

         throw new IllegalArgumentException
            ("cannot subclass without nullary constructor: "
             +superclass.getName());
      }
      
      /**
       * Tell if a given method will be passed by a proxy to its
       * InvocationHandler
       */
      static boolean eligibleForInvocationHandler(Method m)
      {
         int mod = m.getModifiers();

         if (Modifier.isStatic(mod) || Modifier.isFinal(mod))
         {
            // can't override these
            return false;
         }

         if (!Modifier.isAbstract(mod))
         {
            // do not support methods with "super"
            return false;
         }

         return true;
      }
      
      /**
       * Are the 2 methods equal in terms of conflicting with each other.
       * i.e. String toString() and Map toString() are equal since only one
       * toString() can be defined in a class.
       * Also fixes problems with complex inheritance graphs and j2se1.4
       */
      static boolean areEqual(Method m1, Method m2)
      {         
         // Check method names.
         if( ! m1.getName().equals(m2.getName()) )
            return false;
         
         // Check parameters
         Class a1[] = m1.getParameterTypes();
         Class a2[] = m2.getParameterTypes();
         if( a1.length != a2.length )
            return false;
         for( int i=0; i < a1.length; i++)
            if( !a1[i].equals(a1[i]) )
               return false;
         
         return true;
      }

      /**
       * Combine the given list of method[]'s into one method[],
       * removing any methods duplicates.
       */
      static Method[] combineMethodLists(Method methodLists[][])
      {
         int nm = 0;
         for (int i = 0; i < methodLists.length; i++)
         {
            nm += methodLists[i].length;
         }
         Method methods[] = new Method[nm];

         // Merge the methods into a single array.
         nm=0;
         for (int i = 0; i < methodLists.length; i++)
            for (int j = 0; j < methodLists[i].length; j++)
               methods[nm++]=methodLists[i][j];
         
         // Remove duplicate methods. (set them to null)
         for( int i=0; i < methods.length; i++ )
         {
            if( methods[i] == null )
               continue;
            for( int j=i+1; j < methods.length; j++ )
            {
               if( methods[j] == null )
                  continue;
               if( areEqual(methods[i], methods[j]) )
               {
                  methods[j]=null;
                  nm--;
               }
            }
         }
         
         // shorten and copy the array
         ArrayList tmp = new ArrayList();
         for (int i = 0; i < methods.length; i++)
         {
            if( methods[i] != null )
               tmp.add(methods[i]);
         }
         Method methodsCopy[] = new Method[tmp.size()];
         tmp.toArray(methodsCopy);
         return methodsCopy;
      }
      
      Method[] copyMethods()
      {
         return (Method[])methods.clone();
      }

      Class[] copyTargetTypes()
      {
         return (Class[])targetTypes.clone();
      }
      
      ProxyTarget newTarget(InvocationHandler invocationHandler,
                            ClassLoader parent)
         throws Exception
      {
         if (proxyConstructor == null)
         {
            // make the proxy constructor
            ProxyCompiler pc = new ProxyCompiler(parent, 
                                                 superclass,
                                                 targetTypes, 
                                                 methods);

            Class type[] = { InvocationHandler.class };
            proxyConstructor = pc.getProxyType().getConstructor(type);
         }

         Object args[] = { invocationHandler };
         return (ProxyTarget)proxyConstructor.newInstance(args);
      }
      
      ProxyInvocationHandler newInvocationHandler(final Object target)
      {
         if (proxyString == null)
         {
            String s = "InvocationHandler@" + targetTypes[0].getName();
            for (int i = 1; i < targetTypes.length; i++)
            {
               s += "," + targetTypes[i].getName();
            }
            proxyString = s;
         }

         return new ProxyInvocationHandler()
         {
            // (ISSUE: Should this be made subclassable?)
            public Object getTarget()
            {
               return target;
            }
            
            public Class[] getTargetTypes()
            {
               return copyTargetTypes();
            }
            
            public String toString()
            {
               return proxyString + "[" + target + "]";
            }
            
            public Object invoke(Object dummy,
                                 Method method,
                                 Object values[])
               throws Throwable
            {
               return Impl.this.invoke(target, method, values);
            }
         };
      }
      
      /** 
       * The heart of a ProxyInvocationHandler.
       */
      Object invoke(Object target, Member method, Object values[])
         throws Throwable
      {
         // Note:  
         //
         // We will not invoke the method unless we are expecting it.
         // Thus, we cannot blindly call Method.invoke, but must first
         // check our list of allowed methods.
         
         try
         {
            Method methods[] = this.methods; // cache
            
            // use fast pointer equality (it usually succeeds)
            for (int i = methods.length; --i >= 0; )
            {
               if (methods[i] == method)
               {
                  return methods[i].invoke(target, values);
               }
            }
            
            // special case:  allow a null method to select the unique one
            if (method == null)
            {
               if (methods.length == 1)
               {
                  return methods[0].invoke(target, values);
               }
               throw new IllegalArgumentException("non-unique method");
            }
            
            // try the slower form of equality
            for (int i = methods.length; --i >= 0; )
            {
               if (methods[i].equals(method))
               {
                  return methods[i].invoke(target, values);
               }
            }
            
         } 
         catch (InvocationTargetException e)
         {
            throw e.getTargetException();
         }
         
         throw new IllegalArgumentException("method unexpected "+method);
      }
   }
}
