/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.util; 
 
import java.util.Comparator;

/**
 * Data structure that mantains data in a ordered binary tree; each node is
 * greater (smaller) or equal than its 2 sub-nodes, for all the hierarchy. <p>
 * Elements of this data structure should either implement Comparable, or a 
 * Comparator should be given as argument to the constructor.
 *
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.1 $
 */
public class Heap
{
   private Comparator m_comparator;
   private int m_count;
   private Object[] m_nodes;

   /**
    * Creates a new Heap whose elements inserted implement the 
    * {@link Comparable} interface.
    */
   public Heap()
   {
      this(null);
   }
   
   /**
    * Creates a new Heap whose elements are compared using the given
    * {@link Comparator}.
    */
   public Heap(Comparator comparator)
   {
      m_comparator = comparator;
      clear();
   }

   /**
    * Inserts the given element in this heap.
    * 
    * @see #extract
    */
   public void insert(Object obj)
   {
      int length = m_nodes.length;
      // Expand if necessary
      if (m_count == length)
      {
         Object[] newNodes = new Object[length + length];
         System.arraycopy(m_nodes, 0, newNodes, 0, length);
         m_nodes = newNodes;
      }
      // Be cur_slot the first unused slot index; be par_slot its parent index.
      // Start from cur_slot and walk up the tree comparing the object to 
      // insert with the object at par_slot; if it's smaller move down the object at par_slot,
      // otherwise cur_slot is the index where insert the object. If not done, 
      // shift up the tree so that now cur_slot is the old par_slot and 
      // par_slot is the parent index of the new cur_slot (so the grand-parent
      // index of the old cur_slot) and compare again.
      int k = m_count;
      while (k > 0)
      {
         int par = parent(k);
         if (compare(obj, m_nodes[par]) < 0)
         {
            m_nodes[k] = m_nodes[par];
            k = par;
         }
         else break;
      }
      m_nodes[k] = obj;
      ++m_count;
   }
   
   /**
    * Removes and returns the least element of this heap.
    * 
    * @see #insert
    * @see #peek
    */
   public Object extract()
   {
      if (m_count < 1) {return null;}
      else
      {
         int length = m_nodes.length >> 1;
         // Shrink if necessary
         if (length > 5 && m_count < (length >> 1))
         {
            Object[] newNodes = new Object[length];
            System.arraycopy(m_nodes, 0, newNodes, 0, length);
            m_nodes = newNodes;
         }
         //
         int k = 0;
         Object ret = m_nodes[k];
         --m_count;
         Object last = m_nodes[m_count];
         for (;;)
         {
            int l = left(k);
            if (l >= m_count) {break;}
            else
            {
               int r = right(k);
               int child = (r >= m_count || compare(m_nodes[l], m_nodes[r]) < 0) ? l : r;
               if (compare(last, m_nodes[child]) > 0)
               {
                  m_nodes[k] = m_nodes[child];
                  k = child;
               }
               else {break;}
            }
         }
         m_nodes[k] = last;
         m_nodes[m_count] = null;
         return ret;
      }
   }
   
   /**
    * Returns, without removing it, the least element of this heap.
    * 
    * @see #extract
    */
   public Object peek()
   {
      if (m_count < 1) {return null;}
      else {return m_nodes[0];}
   }
   
   /**
    * Empties this heap
    */
   public void clear() 
   {
      m_count = 0;
      m_nodes = new Object[10];
   }

   /**
    * Compares the given objects using the comparator, if available,
    * or considering them Comparable objects.
    * 
    * @throws ClassCastException if nor the comparator is given
    * and nor both objects implements the Comparable interface
    */
   protected int compare(Object o1, Object o2)
   {
      if (m_comparator != null)
      {
         return m_comparator.compare(o1, o2);
      }
      else
      {
         if (o1 == null)
         {
            if (o2 == null) {return 0;}
            else {return -((Comparable)o2).compareTo(o1);}
         }
         else {return ((Comparable)o1).compareTo(o2);}
      }
   }
   
   /**
    * Returns the parent index of <code>index</code>.
    */
   protected int parent(int index)
   {
      return (index - 1) >> 1;
   }
   
   /**
    * Returns the left child index of <code>index</code>.
    */
   protected int left(int index)
   {
      return index + index + 1;
   }
   
   /**
    * Returns the right child index of <code>index</code>.
    */
   protected int right(int index)
   {
      return index + index + 2;
   }
}
