// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: LazyList.java,v 1.15.2.4 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;
import java.io.Serializable;
import java.util.AbstractList;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.ConcurrentModificationException;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.NoSuchElementException;

/* ------------------------------------------------------------ */
/** Lazy List creation.
 * A List helper class that attempts to avoid unneccessary List
 * creation.   If a method needs to create a List to return, but it is
 * expected that this will either be empty or frequently contain a
 * single item, then using LazyList will avoid additional object
 * creations by using Collections.EMPTY_LIST or
 * Collections.singletonList where possible.
 *
 * <p><h4>Usage</h4>
 * <pre>
 *   Object lazylist =null;
 *   while(loopCondition)
 *   {
 *     Object item = getItem();
 *     if (item.isToBeAdded())
 *         lazylist = LazyList.add(lazylist,item);
 *   }
 *   return LazyList.getList(lazylist);
 * </pre>
 *
 * An ArrayList of default size is used as the initial LazyList.
 *
 * @see java.util.List
 * @version $Revision: 1.15.2.4 $
 * @author Greg Wilkins (gregw)
 */
public class LazyList
    implements Cloneable, Serializable
{
    private static final String[] __EMTPY_STRING_ARRAY = new String[0];
    
    /* ------------------------------------------------------------ */
    private LazyList()
    {}
    
    /* ------------------------------------------------------------ */
    /** Add an item to a LazyList 
     * @param list The list to add to or null if none yet created.
     * @param item The item to add.
     * @return The lazylist created or added to.
     */
    public static Object add(Object list, Object item)
    {
        if (list==null)
        {
            if (item instanceof List || item==null)
            {
                List l = new ArrayList();
                l.add(item);
                return l;
            }

            return item;
        }

        if (list instanceof List)
        {
            ((List)list).add(item);
            return list;
        }

        List l=new ArrayList();
        l.add(list);
        l.add(item);
        return l;    
    }
    
    /* ------------------------------------------------------------ */
    /** Add the contents of a Collection to a LazyList
     * @param list The list to add to or null if none yet created.
     * @param collection The Collection whose contents should be added.
     * @return The lazylist created or added to.
     */
    public static Object add(Object list, Collection collection)
    {
        Iterator i=collection.iterator();
        while(i.hasNext())
            list=LazyList.add(list,i.next());
        return list;
    }

    /* ------------------------------------------------------------ */
    /** Add an item to a LazyList 
     * @param list The list to add to or null if none yet created.
     * @param initialSize A size to use when creating the real list
     * @param item The item to add.
     * @return The lazylist created or added to.
     */
    public static Object add(Object list, int initialSize, Object item)
    {
        if (list==null)
        {
            if (item instanceof List || item==null)
            {
                List l = new ArrayList(initialSize);
                l.add(item);
                return l;
            }

            return item;
        }

        if (list instanceof List)
        {
            ((List)list).add(item);
            return list;
        }

        List l=new ArrayList(initialSize);
        l.add(list);
        l.add(item);
        return l;    
    }

    /* ------------------------------------------------------------ */
    public static Object remove(Object list, Object o)
    {
        if (list==null)
            return null;

        if (list instanceof List)
        {
            List l = (List)list;
            l.remove(o);
            if (l.size()==0)
                return null;
            return list;
        }

        return null;
    }
    
    
    /* ------------------------------------------------------------ */
    /** Get the real List from a LazyList.
     * 
     * @param list A LazyList returned from LazyList.add(Object)
     * @return The List of added items, which may be an EMPTY_LIST
     * or a SingletonList.
     */
    public static List getList(Object list)
    {
        return getList(list,false);
    }
    

    /* ------------------------------------------------------------ */
    /** Get the real List from a LazyList.
     * 
     * @param list A LazyList returned from LazyList.add(Object) or null
     * @param nullForEmpty If true, null is returned instead of an
     * empty list.
     * @return The List of added items, which may be null, an EMPTY_LIST
     * or a SingletonList.
     */
    public static List getList(Object list, boolean nullForEmpty)
    {
        if (list==null)
            return nullForEmpty?null:Collections.EMPTY_LIST;
        if (list instanceof List)
            return (List)list;
        
        List l = new ArrayList(1);
        l.add(list);
        return l;
    }

    
    /* ------------------------------------------------------------ */
    public static String[] toStringArray(Object list)
    {
        if (list==null)
            return __EMTPY_STRING_ARRAY;
        
        if (list instanceof List)
        {
            List l = (List)list;
            
            String[] a = new String[l.size()];
            for (int i=l.size();i-->0;)
            {
                Object o=l.get(i);
                if (o!=null)
                    a[i]=o.toString();
            }
            return a;
        }
        
        return new String[] {list.toString()};
    }


    /* ------------------------------------------------------------ */
    /** The size of a lazy List 
     * @param list  A LazyList returned from LazyList.add(Object) or null
     * @return the size of the list.
     */
    public static int size(Object list)
    {
        if (list==null)
            return 0;
        if (list instanceof List)
            return ((List)list).size();
        return 1;
    }
    
    /* ------------------------------------------------------------ */
    /** Get item from the list 
     * @param list  A LazyList returned from LazyList.add(Object) or null
     * @param i int index
     * @return the item from the list.
     */
    public static Object get(Object list, int i)
    {
        if (list==null)
            throw new IndexOutOfBoundsException();
        
        if (list instanceof List)
            return ((List)list).get(i);

        if (i==0)
            return list;
        
        throw new IndexOutOfBoundsException();
    }
    
    /* ------------------------------------------------------------ */
    public static boolean contains(Object list,Object item)
    {
        if (list==null)
            return false;
        
        if (list instanceof List)
            return ((List)list).contains(item);

        return list.equals(item);
    }
    

    /* ------------------------------------------------------------ */
    public static Object clone(Object list)
    {
        if (list==null)
            return null;
        if (list instanceof List)
            return new ArrayList((List)list);
        return list;
    }
    
    /* ------------------------------------------------------------ */
    public static String toString(Object list)
    {
        if (list==null)
            return "[]";
        if (list instanceof List)
            return ((List)list).toString();
        return "["+list+"]";
    }

    /* ------------------------------------------------------------ */
    public static Iterator iterator(Object list)
    {
        if (list==null)
            return Collections.EMPTY_LIST.iterator();
        if (list instanceof List)
            return ((List)list).iterator();
        return getList(list).iterator();
    }
    
    /* ------------------------------------------------------------ */
    public static ListIterator listIterator(Object list)
    {
        if (list==null)
            return Collections.EMPTY_LIST.listIterator();
        if (list instanceof List)
            return ((List)list).listIterator();
        return getList(list).listIterator();
    }
    
}

