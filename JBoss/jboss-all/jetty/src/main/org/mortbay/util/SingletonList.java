// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: SingletonList.java,v 1.15.2.3 2003/06/04 04:47:59 starksm Exp $
// ========================================================================

package org.mortbay.util;
import java.util.AbstractList;
import java.util.Iterator;
import java.util.ListIterator;
import java.util.NoSuchElementException;

/* ------------------------------------------------------------ */
/** Singleton List.
 * This simple efficient implementation of a List with a single
 * element is provided for JDK 1.2 JVMs, which do not provide
 * the Collections.singletonList method.
 *
 * @version $Revision: 1.15.2.3 $
 * @author Greg Wilkins (gregw)
 */
public class SingletonList extends AbstractList
{
    private Object o;
    
    /* ------------------------------------------------------------ */
    private SingletonList(Object o)
    {
        this.o=o;
    }

    /* ------------------------------------------------------------ */
    public static SingletonList newSingletonList(Object o)
    {
        return new SingletonList(o);
    }

    /* ------------------------------------------------------------ */
    public Object get(int i)
    {
        if (i!=0)
            throw new IndexOutOfBoundsException("index "+i);
        return o;
    }

    /* ------------------------------------------------------------ */
    public int size()
    {
        return 1;
    }

    /* ------------------------------------------------------------ */
    public ListIterator listIterator()
    {
        return new SIterator();
    }
    
    /* ------------------------------------------------------------ */
    public ListIterator listIterator(int i)
    {
        return new SIterator(i);
    }
    
    /* ------------------------------------------------------------ */
    public Iterator iterator()
    {
        return new SIterator();
    }


    /* ------------------------------------------------------------ */
    private class SIterator implements ListIterator
    {
        int i;
        
        SIterator(){i=0;}
        SIterator(int i)
        {
            if (i<0||i>1)
                throw new IndexOutOfBoundsException("index "+i);
            this.i=i;
        }
        public void add(Object o){throw new UnsupportedOperationException("SingletonList.add()");}
        public boolean hasNext() {return i==0;}
        public boolean hasPrevious() {return i==1;}
        public Object next() {if (i!=0) throw new NoSuchElementException();i++;return o;}
        public int nextIndex() {return i;}
        public Object previous() {if (i!=1) throw new NoSuchElementException();i--;return o;}
        public int previousIndex() {return i-1;}
        public void remove(){throw new UnsupportedOperationException("SingletonList.remove()");}
        public void set(Object o){throw new UnsupportedOperationException("SingletonList.add()");}
    }
}
