/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jnp.interfaces;

/**
 * This class exists because the JNDI API set wisely uses java.util.Properties
 * which extends Hashtable, a threadsafe class. The NamingParser uses a static
 * instance, making it a global source of contention. This results in
 * a huge scalability problem, which can be seen in ECPerf, as sometimes half
 * of the worker threads are stuck waiting for this stupid lock, sometimes
 * themselves holdings global locks, e.g. to the AbstractInstanceCache.
 *
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * @author <a href="mailto:sreich@apple.com">Stefan Reich</a>
 */
class FastNamingProperties extends java.util.Properties
{
   FastNamingProperties()
   {
   }
   public Object setProperty(String s1, String s2)
   {
      throw new UnsupportedOperationException();
   }
   public void load(java.io.InputStream is) throws java.io.IOException 
   {
      throw new UnsupportedOperationException();
   }

   public String getProperty(String s)
   {
      if(s.equals("jndi.syntax.direction"))
      {
         return "left_to_right";
      }
      else if (s.equals("jndi.syntax.ignorecase"))
      {
         return "false";
      }
      else if (s.equals("jndi.syntax.separator"))
      {
         return "/";
      }
      else
      {
         return null;
      }	
   }

   public String getProperty(String name, String defaultValue)
   {
      String ret = getProperty(name);
      if (ret==null)
      {
         ret=defaultValue;
      }
      return ret;
   }

   public java.util.Enumeration propertyNames()
   {
      throw new UnsupportedOperationException();
   }

   public void list(java.io.PrintStream ps)
   {
      throw new UnsupportedOperationException();
   }

   public void list(java.io.PrintWriter ps)
   {
      throw new UnsupportedOperationException();
   }

   // methods from Hashtable

   public int size()
   {
      throw new UnsupportedOperationException();
   }

   public boolean isEmpty()
   {
      throw new UnsupportedOperationException();
   }

   public java.util.Enumeration keys()
   {
      throw new UnsupportedOperationException();
   }

   public java.util.Enumeration elements()
   {
      throw new UnsupportedOperationException();
   }

   public boolean contains(java.lang.Object o)
   {
      throw new UnsupportedOperationException();
   }

   public boolean containsValue(java.lang.Object o)
   {
      throw new UnsupportedOperationException();
   }

   public boolean containsKey(java.lang.Object o)
   {
      throw new UnsupportedOperationException();
   }

   public java.lang.Object get(java.lang.Object o)
   {
      throw new UnsupportedOperationException();
   }

   public java.lang.Object put(java.lang.Object o1, java.lang.Object o2)
   {
      throw new UnsupportedOperationException();
   }

   public java.lang.Object remove(java.lang.Object o)
   {
      throw new UnsupportedOperationException();
   }

   public void putAll(java.util.Map m)
   {
      throw new UnsupportedOperationException();
   }

   public void clear()
   {
      throw new UnsupportedOperationException();
   }

   public java.lang.Object clone()
   {
      throw new UnsupportedOperationException();
   }

   public String toString()
   {
      throw new UnsupportedOperationException();
   }

   public java.util.Set keySet()
   {
      throw new UnsupportedOperationException();
   }

   public java.util.Set entrySet()
   {
      throw new UnsupportedOperationException();
   }

   public java.util.Collection values()
   {
      throw new UnsupportedOperationException();
   }

   public boolean equals(java.lang.Object o)
   {
      throw new UnsupportedOperationException();
   }

   public int hashCode()
   {
      throw new UnsupportedOperationException();
   }
}
