/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.util.collection;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Enumeration;
import java.util.NoSuchElementException;

/**
 * ???
 *      
 * @author ???
 * @version $Revision: 1.1 $
 */
public class SerializableEnumeration
   extends ArrayList
   implements Enumeration
{
   private int index;

   public SerializableEnumeration () {
      index = 0;
   }

   public SerializableEnumeration (Collection c) {
      super(c);
      index = 0;
   }
	 
   public SerializableEnumeration (int initialCapacity) {
      super(initialCapacity);
      index = 0;
   }
	 
   public boolean hasMoreElements() {
      return (index < size());
   }
	
   public Object nextElement() throws NoSuchElementException
   {
      try {
         Object nextObj = get(index);
         index++;
         return nextObj;
      }
      catch (IndexOutOfBoundsException e) {
         throw new NoSuchElementException();
      }
   }

   private void writeObject(java.io.ObjectOutputStream out)
      throws java.io.IOException
   {
      // the only thing to write is the index field
      out.defaultWriteObject();
   }
   
   private void readObject(java.io.ObjectInputStream in)
      throws java.io.IOException, ClassNotFoundException
   {
      in.defaultReadObject();
   }
}
