// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: Pool.java,v 1.1.2.6 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;

import java.io.Serializable;
import java.util.HashMap;

/* ------------------------------------------------------------ */
/** A pool of Objects.
 * <p>
 * @version $Id: Pool.java,v 1.1.2.6 2003/06/04 04:47:58 starksm Exp $
 * @author Juancarlo Añez <juancarlo@modelistica.com>
 * @author Greg Wilkins <gregw@mortbay.com>
 */
public class Pool
    implements LifeCycle, Serializable
{
    /* ------------------------------------------------------------ */
    static int __max = 
        Integer.getInteger("POOL_MAX",256).intValue();
    static int __min =
        Integer.getInteger("POOL_MIN",2).intValue();

    
    /* ------------------------------------------------------------ */
    public static interface PondLife
    {
        int getID();
        void enterPool(Pool pool,int id);
        void poolClosing();
        void leavePool();
    }
    
    /* ------------------------------------------------------------------- */
    static HashMap __nameMap=new HashMap();
    
    /* ------------------------------------------------------------------- */
    private int _max = __max;
    private int _min = __min;
    private String _name;
    private String _className;
    private int _maxIdleTimeMs=10000;
    private HashMap _attributes = new HashMap();
    
    private transient Class _class;
    private transient PondLife[] _pondLife;
    private transient int[] _index;
    private transient int _size;
    private transient int _available;
    private transient int _running=0;
    
    /* ------------------------------------------------------------------- */
    public static Pool getPool(String name)
    {
        return (Pool)__nameMap.get(name);
    }

    /* ------------------------------------------------------------------- */
    /* Construct
     */
    public Pool() 
    {}

    /* ------------------------------------------------------------ */
    /** 
     * @return The name of the Pool.
     */
    public String getPoolName()
    {
        return _name;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @param name The pool name
     * @exception IllegalStateException If the name is already defined.
     */
    public void setPoolName(String name)
        throws IllegalStateException
    {
        synchronized(Pool.class)
        {
            if (_name!=null && !_name.equals(name))
                __nameMap.remove(_name);
            if (__nameMap.containsKey(name))
                throw new IllegalStateException("Name already exists");
            _name=name;
            __nameMap.put(_name,this);
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Set the class.
     * @param poolClass The class
     * @exception IllegalStateException If the pool has already
     *            been started.
     */
    public void setPoolClass(Class poolClass)
        throws IllegalStateException
    {
        if (_class!=poolClass)
        {
            if (_running>0)
                throw new IllegalStateException("Thread Pool Running");
            if (!PondLife.class.isAssignableFrom(poolClass))
                throw new IllegalArgumentException("Not PondLife: "+poolClass);
            _class=poolClass;
            _className=_class.getName();
        }
    }
    

    /* ------------------------------------------------------------ */
    public Class getPoolClass()
    {
        return _class;
    }

    /* ------------------------------------------------------------ */
    public int getMinSize()
    {
        return _min;
    }
    
    /* ------------------------------------------------------------ */
    public void setMinSize(int min)
    {
        _min=min;
    }
    
    /* ------------------------------------------------------------ */
    public int getMaxSize()
    {
        return _max;
    }
    
    /* ------------------------------------------------------------ */
    public void setMaxSize(int max)
    {
        _max=max;
    }
    
    /* ------------------------------------------------------------ */
    public int getMaxIdleTimeMs()
    {
        return _maxIdleTimeMs;
    }
    
    /* ------------------------------------------------------------ */
    public void setMaxIdleTimeMs(int maxIdleTimeMs)
    {
        _maxIdleTimeMs=maxIdleTimeMs;
    }
    
    /* ------------------------------------------------------------ */
    public void setAttribute(String name,Object value)
    {
        _attributes.put(name,value);
    }
    
    /* ------------------------------------------------------------ */
    public Object getAttribute(String name)
    {
        return _attributes.get(name);
    }
    
    /* ------------------------------------------------------------ */
    public boolean isStarted()
    {
        return _running>0 && _pondLife!=null;
    }
    
    /* ------------------------------------------------------------ */
    public int size()
    {
        return _size;
    }
    
    /* ------------------------------------------------------------ */
    public int available()
    {
        return _available;
    }
    
    
    /* ------------------------------------------------------------ */
    public void start()
        throws Exception
    {
        synchronized(this)
        {
            _running++;
            if (_running>1)
                return;

            // Start the threads
            _pondLife=new PondLife[_max];
            _index=new int[_max];
            _size=0;
            _available=0;
            
            for (int i=0;i<_max;i++)
                _index[i]=i;
            for (int i=0;i<_min;i++)
                newPondLife();
        }
    }

    /* ------------------------------------------------------------ */
    public void stop()
        throws InterruptedException
    {
        synchronized(this)
        {
            _running--;
            if (_running>0)
                return;
            notifyAll();
        }
        
        if (_pondLife!=null && _size>0)
        {
            for (int i=0;i<_pondLife.length;i++)
                closePondLife(i);
            Thread.yield();
            for (int i=0;i<_pondLife.length;i++)
                stopPondLife(i);
        }
        _pondLife=null;
        _index=null;
        _size=0;
        _available=0;
    }
    
    /* ------------------------------------------------------------ */
    public PondLife get(int timeoutMs)
        throws Exception
    {
        PondLife pl=null;
        
        // Defer to other threads before locking 
        if (_available<_min)
            Thread.yield();
        
        int new_id=-1;
        
        // Try to get pondlife without creating new one.
        synchronized(this)
        {
            // Wait if none available.
            if (_running>0 && _available==0 && _size==_pondLife.length && timeoutMs>0)
                wait(timeoutMs);

            // If still running
            if (_running>0)
            {
                // if pondlife available
                if (_available>0)
                {
                    int id=_index[--_available];
                    pl=_pondLife[id];
                }
                else if (_size<_pondLife.length)
                {
                    // Reserve spot for a new one
                    new_id=reservePondLife(false);
                }
            }
        }

        // create reserved pondlife
        if (pl==null && new_id>=0)
            pl=newPondLife(new_id);

        return pl;
    }
    
    
    /* ------------------------------------------------------------ */
    public void put(PondLife pl)
        throws InterruptedException
    {
        int id=pl.getID();
        
        if (_running==0)
            stopPondLife(id);
        else
        {
            synchronized(this)
            {
                if (_running==0)
                    stopPondLife(id);
                else if (_pondLife[id]!=null)
                {
                    _index[_available++]=id;
                    notify();
                }
            }
        }
    }
    
    /* ------------------------------------------------------------ */
    public void shrink()
        throws InterruptedException
    {
        if (_running==0)
            return;

        synchronized(this)
        {
            if (_running>0 && _available>0 && _size>_min)
                stopPondLife(_index[--_available]);
        }
    }

    /* ------------------------------------------------------------ */
    private int reservePondLife(boolean available)
        throws Exception
    {
        int id=-1;
        synchronized(this)
        {
            id=_index[_size++];
            if (available)
                _index[_available++]=id;
        }
        return id;
    }
    
    /* ------------------------------------------------------------ */
    private PondLife newPondLife(int id)
        throws Exception
    {
        PondLife pl= (PondLife)_class.newInstance();
        _pondLife[id]=pl;
        pl.enterPool(this,id);
        return pl;
    }

    /* ------------------------------------------------------------ */
    private PondLife newPondLife()
        throws Exception
    {
        return newPondLife(reservePondLife(true));
    }
    
    /* ------------------------------------------------------------ */
    private void closePondLife(int id)
    {
        if (_pondLife[id]!=null)
            _pondLife[id].poolClosing();
    }
    
    /* ------------------------------------------------------------ */
    private void stopPondLife(int id)
    {
        PondLife pl = null;
        synchronized(this)
        {
            pl=_pondLife[id];
            if (pl!=null)
            {
                _pondLife[id]=null;
                _index[--_size]=id;
                if (_available>_size)
                    _available=_size;
            }
        }
        if (pl!=null)
            pl.leavePool();
    }

    /* ------------------------------------------------------------ */
    public void dump(String msg)
    {
        StringBuffer pond=new StringBuffer();
        StringBuffer avail=new StringBuffer();
        StringBuffer index=new StringBuffer();
        
         pond.append("pond: ");
         avail.append("avail:");
         index.append("index:");
        
        for (int i=0;i<_pondLife.length;i++)
        {
            if (_pondLife[i]==null)
                pond.append("   ");
            else
            {
                pond.append(' ');
                StringUtil.append(pond,(byte)i,16);
            }
            
            if (i==_size)
                avail.append(i==_available?" AS":"  S");
            else
                avail.append(i==_available?" A ":"   ");
            
            index.append(' ');
            StringUtil.append(index,(byte)_index[i],16);
        }

        System.err.println();
        System.err.println(msg);
        System.err.println(pond);
        System.err.println(avail);
        System.err.println(index);
    }
    
    
    /* ------------------------------------------------------------ */
    private void readObject(java.io.ObjectInputStream in)
        throws java.io.IOException, ClassNotFoundException
    {
        in.defaultReadObject();
        if (_class==null || !_class.getName().equals(_className))
        {
            try
            {
                setPoolClass(Loader.loadClass(Pool.class,_className));
            }
            catch (Exception e)
            {
                Code.warning(e);
                throw new java.io.InvalidObjectException(e.toString());
            }
        }
    }
}
