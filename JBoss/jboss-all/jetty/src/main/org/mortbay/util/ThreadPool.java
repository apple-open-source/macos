// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ThreadPool.java,v 1.15.2.9 2003/06/04 04:47:59 starksm Exp $
// ========================================================================

package org.mortbay.util;

import java.io.Serializable;

/* ------------------------------------------------------------ */
/** A pool of threads.
 * <p>
 * Avoids the expense of thread creation by pooling threads after
 * their run methods exit for reuse.
 * <p>
 * If the maximum pool size is reached, jobs wait for a free thread.
 * By default there is no maximum pool size.  Idle threads timeout
 * and terminate until the minimum number of threads are running.
 * <p>
 * This implementation uses the run(Object) method to place a
 * job on a queue, which is read by the getJob(timeout) method.
 * Derived implementations may specialize getJob(timeout) to
 * obtain jobs from other sources without queing overheads.
 *
 * @version $Id: ThreadPool.java,v 1.15.2.9 2003/06/04 04:47:59 starksm Exp $
 * @author Juancarlo Añez <juancarlo@modelistica.com>
 * @author Greg Wilkins <gregw@mortbay.com>
 */
public class ThreadPool
    implements LifeCycle, Serializable
{
    public static final String __DAEMON="org.mortbay.util.ThreadPool.daemon";
    
    /* ------------------------------------------------------------------- */
    private String _name;
    private Pool _pool;
    private Object _join="";

    private transient boolean _started;
    
    /* ------------------------------------------------------------------- */
    /* Construct
     */
    public ThreadPool()
    {
        _pool=new Pool();
        _pool.setPoolClass(ThreadPool.PoolThread.class);
        _name=this.getClass().getName();
        int dot=_name.lastIndexOf('.');
        if (dot>=0)
            _name=_name.substring(dot+1);
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return The name of the ThreadPool.
     */
    public String getName()
    {
        return _name;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @param name Name of the ThreadPool to use when naming Threads.
     */
    public void setName(String name)
    {
        _name=name;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return Name of the Pool instance this ThreadPool uses or null for
     * an anonymous private pool.
     */
    public String getPoolName()
    {
        return _pool.getPoolName();
    }

    /* ------------------------------------------------------------ */
    /** Set the Pool name.
     * All ThreadPool instances with the same Pool name will share the
     * same Pool instance. Thus they will share the same max, min and
     * available Threads.  The field values of the first ThreadPool to call
     * setPoolName with a specific name are used for the named
     * Pool. Subsequent ThreadPools that join the name pool will loose their
     * private values.
     * @param name Name of the Pool instance this ThreadPool uses or null for
     * an anonymous private pool.
     */
    public void setPoolName(String name)
    {
        synchronized(Pool.class)
        {
            if (isStarted())
            {
                if ((name==null && _pool.getPoolName()!=null) ||
                    (name!=null && !name.equals(_pool.getPoolName())))
                    throw new IllegalStateException("started");
                return;
            }
            
            if (name==null)
            {
                if (_pool.getPoolName()!=null)
                    _pool=new Pool();
            }
            else
            {
                Pool pool=Pool.getPool(name);
                if (pool==null)
                    _pool.setPoolName(name);
                else
                    _pool=pool;
            }
        }
    }

    /* ------------------------------------------------------------ */
    /** 
     * Delegated to the named or anonymous Pool.
     */
    public boolean isDaemon()
    {
        return _pool.getAttribute(__DAEMON)!=null;
    }

    /* ------------------------------------------------------------ */
    /** 
     * Delegated to the named or anonymous Pool.
     */
    public void setDaemon(boolean daemon)
    {
        _pool.setAttribute(__DAEMON,daemon?"true":null);
    }
    
    /* ------------------------------------------------------------ */
    /** Is the pool running jobs.
     * @return True if start() has been called.
     */
    public boolean isStarted()
    {
        return _started;
    }
    
    /* ------------------------------------------------------------ */
    /** Get the number of threads in the pool.
     * Delegated to the named or anonymous Pool.
     * @see #getIdleThreads
     * @return Number of threads
     */
    public int getThreads()
    {
        return _pool.size();
    }
    
    /* ------------------------------------------------------------ */
    /** Get the number of idle threads in the pool.
     * Delegated to the named or anonymous Pool.
     * @see #getThreads
     * @return Number of threads
     */
    public int getIdleThreads()
    {
        return _pool.available();
    }
    
    /* ------------------------------------------------------------ */
    /** Get the minimum number of threads.
     * Delegated to the named or anonymous Pool.
     * @see #setMinThreads
     * @return minimum number of threads.
     */
    public int getMinThreads()
    {
        return _pool.getMinSize();
    }
    
    /* ------------------------------------------------------------ */
    /** Set the minimum number of threads.
     * Delegated to the named or anonymous Pool.
     * @see #getMinThreads
     * @param minThreads minimum number of threads
     */
    public void setMinThreads(int minThreads)
    {
        _pool.setMinSize(minThreads);
    }
    
    /* ------------------------------------------------------------ */
    /** Set the maximum number of threads.
     * Delegated to the named or anonymous Pool.
     * @see #setMaxThreads
     * @return maximum number of threads.
     */
    public int getMaxThreads()
    {
        return _pool.getMaxSize();
    }
    
    /* ------------------------------------------------------------ */
    /** Set the maximum number of threads.
     * Delegated to the named or anonymous Pool.
     * @see #getMaxThreads
     * @param maxThreads maximum number of threads.
     */
    public void setMaxThreads(int maxThreads)
    {
        _pool.setMaxSize(maxThreads);
    }
    
    /* ------------------------------------------------------------ */
    /** Get the maximum thread idle time.
     * Delegated to the named or anonymous Pool.
     * @see #setMaxIdleTimeMs
     * @return Max idle time in ms.
     */
    public int getMaxIdleTimeMs()
    {
        return _pool.getMaxIdleTimeMs();
    }
    
    /* ------------------------------------------------------------ */
    /** Set the maximum thread idle time.
     * Threads that are idle for longer than this period may be
     * stopped.
     * Delegated to the named or anonymous Pool.
     * @see #getMaxIdleTimeMs
     * @param maxIdleTimeMs Max idle time in ms.
     */
    public void setMaxIdleTimeMs(int maxIdleTimeMs)
    {
        _pool.setMaxIdleTimeMs(maxIdleTimeMs);
    }
    
    
    /* ------------------------------------------------------------ */
    /** Set Max Read Time.
     * @deprecated maxIdleTime is used instead.
     */
    public void setMaxStopTimeMs(int ms)
    {
        Code.warning("setMaxStopTimeMs is deprecated. No longer required.");
    }
    
    /* ------------------------------------------------------------ */
    /* Start the ThreadPool.
     * Construct the minimum number of threads.
     */
    public void start()
        throws Exception
    {
        _started=true;
        _pool.start();
    }

    /* ------------------------------------------------------------ */
    /** Stop the ThreadPool.
     * New jobs are no longer accepted,idle threads are interrupted
     * and stopJob is called on active threads.
     * The method then waits 
     * min(getMaxStopTimeMs(),getMaxIdleTimeMs()), for all jobs to
     * stop, at which time killJob is called.
     */
    public void stop()
        throws InterruptedException
    {
        _started=false;
        _pool.stop();
        synchronized(_join)
        {
            _join.notifyAll();
        }
    }
    
    /* ------------------------------------------------------------ */
    public void join()
    {
        while(isStarted() && _pool!=null)
        {
            synchronized(_join)
            {
                try{if (isStarted() && _pool!=null)_join.wait(30000);}
                catch (Exception e)
                {
                    e.printStackTrace();
                    Code.ignore(e);
                }
            }
        }
    }
    
    /* ------------------------------------------------------------ */
    public void shrink()
        throws InterruptedException
    {
        _pool.shrink();
    }
    

    /* ------------------------------------------------------------ */
    /** Run job.
     * Give a job to the pool. 
     * @param job  If the job is derived from Runnable, the run method
     * is called, otherwise it is passed as the argument to the handle
     * method.
     */
    public void run(Object job)
        throws InterruptedException
    {
        if (job==null)
            return;
        
        try
        {
            PoolThread thread=(PoolThread)_pool.get(getMaxIdleTimeMs());
            
            if (thread!=null)
                thread.run(this,job);
            else
            {
                Code.warning("No thread for "+job);
                stopJob(null,job);
            }
        }
        catch (InterruptedException e) {throw e;}
        catch (Exception e){Code.warning(e);}
    }
    

    /* ------------------------------------------------------------ */
    /** Handle a job.
     * Called by the allocated thread to handle a job. If the job is a
     * Runnable, it's run method is called. Otherwise this method needs to be
     * specialized by a derived class to provide specific handling.
     * @param job The job to execute.
     * @exception InterruptedException 
     */
    protected void handle(Object job)
        throws InterruptedException
    {
        if (job!=null && job instanceof Runnable)
            ((Runnable)job).run();
        else
            Code.warning("Invalid job: "+job);
    }
    
    /* ------------------------------------------------------------ */
    /** Stop a Job.
     * This method is called by the Pool if a job needs to be stopped.
     * The default implementation does nothing and should be extended by a
     * derived thread pool class if special action is required.
     * @param thread The thread allocated to the job, or null if no thread allocated.
     * @param job The job object passed to run.
     */
    protected void stopJob(Thread thread, Object job)
    {
    }
    
    /* ------------------------------------------------------------ */
    /** Pool Thread class.
     * The PoolThread allows the threads job to be
     * retrieved and active status to be indicated.
     */
    public static class PoolThread extends Thread implements Pool.PondLife
    {
        ThreadPool _threadPool;
        Pool _pool;
        Object _job;
        int _id;
        String _name;
        
        /* ------------------------------------------------------------ */
        public void enterPool(Pool pool,int id)
        {
            _pool=pool;
            _id=id;
            _name=_pool.getPoolName()==null
                ?("PoolThread-"+id):(_pool.getPoolName()+"-"+id);
            this.setName(_name);
            this.setDaemon(pool.getAttribute(__DAEMON)!=null);
            this.start();
            if (Code.verbose())Code.debug("enterPool ",this," -> ",pool);
        }

        /* ------------------------------------------------------------ */
        public int getID()
        {
            return _id;
        }
        
        /* ------------------------------------------------------------ */
        public void poolClosing()
        {
            synchronized(this)
            {
                _pool=null;
                if (_job==null)
                    notify();
                else
                    interrupt();
            }
        }
        /* ------------------------------------------------------------ */
        public void leavePool()
        {
            if (Code.verbose())Code.debug("leavePool ",this," <- ",_pool);
            synchronized(this)
            {
                _pool=null;
                if (_job==null || _threadPool==null)
                    notify();
                else
                {
                    _threadPool.stopJob(this,_job);
                    _job=null;
                }
            }
        }
        
        
        /* ------------------------------------------------------------ */
        public void run(ThreadPool pool, Object job)
        {
            synchronized(this)
            {
                _threadPool=pool;
                _job=job;
                notify();
            }
        }
        
        /* ------------------------------------------------------------ */
        /** ThreadPool run.
         * Loop getting jobs and handling them until idle or stopped.
         */
        public void run() 
        {
            while (_pool!=null && _pool.isStarted())
            {
                try
                { 
                    synchronized(this)
                    {
                        // Wait for a job.
                        if (_pool!=null && _pool.isStarted() && _job==null)
                            wait(_pool.getMaxIdleTimeMs());
                    }
                    
                    // handle
                    if (_job!=null)
                        _threadPool.handle(_job);
                }
                catch (InterruptedException e)
                {
                    Code.ignore(e);
                }
                finally
                {
                    synchronized(this)
                    {
                        boolean got=_job!=null;
                        _job=null;
                        _threadPool=null;
                        try
                        {
                            if (got&&_pool!=null)
                                _pool.put(this);
                        }
                        catch (InterruptedException e){Code.ignore(e);}
                    }
                }
            }
        }

        public String toString()
        {
            return _name;
        }
    }    
}

