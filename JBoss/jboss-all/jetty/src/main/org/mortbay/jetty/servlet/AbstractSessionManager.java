// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: AbstractSessionManager.java,v 1.15.2.16 2003/06/19 00:02:14 slaboure Exp $
// ---------------------------------------------------------------------------

package org.mortbay.jetty.servlet;

import java.util.ArrayList;
import java.util.Collections;
import java.util.ConcurrentModificationException;
import java.util.Enumeration;
import java.util.EventListener;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Random;
import javax.servlet.ServletContext;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionAttributeListener;
import javax.servlet.http.HttpSessionBindingEvent;
import javax.servlet.http.HttpSessionBindingListener;
import javax.servlet.http.HttpSessionContext; // Deprecated but required to implement SessionManager
import javax.servlet.http.HttpSessionEvent;
import javax.servlet.http.HttpSessionListener;
import org.mortbay.util.Code;
import org.mortbay.util.LazyList;


/* ------------------------------------------------------------ */
/** An Abstract implementation of SessionManager.
 * The partial implementation of SessionManager interface provides
 * the majority of the handling required to implement a
 * SessionManager.  Concrete implementations of SessionManager based
 * on AbstractSessionManager need only implement the newSession method
 * to return a specialized version of the Session inner class that
 * provides an attribute Map.
 * <p>
 * If the property
 * org.mortbay.jetty.servlet.AbstractSessionManager.23Notifications is set to
 * true, the 2.3 servlet spec notification style will be used.
 * <p>
 * @version $Id: AbstractSessionManager.java,v 1.15.2.16 2003/06/19 00:02:14 slaboure Exp $
 * @author Greg Wilkins (gregw)
 */
public abstract class AbstractSessionManager implements SessionManager
{
    /* ------------------------------------------------------------ */
    public final static int __distantFuture = 60*60*24*7*52*20;

    /* ------------------------------------------------------------ */
    public final static boolean __24SessionDestroyed=
        Boolean.getBoolean("org.mortbay.jetty.servlet.AbstractSessionManager.24SessionDestroyed");
    
    /* ------------------------------------------------------------ */
    // Setting of max inactive interval for new sessions
    // -1 means no timeout
    private int _dftMaxIdleSecs = -1;
    private int _scavengePeriodMs = 30000;
    private String _workerName ;
    private boolean _useRequestedId=true;
    protected transient ArrayList _sessionListeners=new ArrayList();
    protected transient ArrayList _sessionAttributeListeners=new ArrayList();
    protected transient Map _sessions;
    protected transient Random _random;
    protected transient ServletHandler _handler;
    protected int _minSessions = 0;
    protected int _maxSessions = 0;

    private transient SessionScavenger _scavenger = null;
    
    /* ------------------------------------------------------------ */
    public AbstractSessionManager()
    {
        this(null);
    }
    
    /* ------------------------------------------------------------ */
    public AbstractSessionManager(Random random)
    {
        _random=random;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return True if requested session ID are first considered for new
     * session IDs
     */
    public boolean getUseRequestedId()
    {
        return _useRequestedId;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @param useRequestedId True if requested session ID are first considered for new
     * session IDs
     */
    public void setUseRequestedId(boolean useRequestedId)
    {
        _useRequestedId = useRequestedId;
    }
    
    /* ------------------------------------------------------------ */
    public void initialize(ServletHandler handler)
    {
        _handler=handler;
    }
    
    /* ------------------------------------------------------------ */
    public Map getSessionMap()
    {
        return Collections.unmodifiableMap(_sessions);
    }

   /* ------------------------------------------------------------ */
   public int getSessions ()
   {
      return _sessions.size ();
   }

   /* ------------------------------------------------------------ */
   public int getMinSessions ()
   {
      return _minSessions;
   }

   /* ------------------------------------------------------------ */
   public int getMaxSessions ()
   {
      return _maxSessions;
   }

   /* ------------------------------------------------------------ */
   public void resetStats ()
   {
      _minSessions =  _sessions.size ();
      _maxSessions = _sessions.size ();
   }

    /* ------------------------------------------------------------ */
    /* new Session ID.
     * If the request has a requestedSessionID which is unique, that is used.
     * The session ID is created as a unique random long, represented as in a
     * base between 30 and 36, selected by timestamp.
     * If the request has a jvmRoute attribute, that is appended as a
     * worker tag, else any worker tag set on the manager is appended.
     * @param request 
     * @param created 
     * @return Session ID.
     */
    private String newSessionId(HttpServletRequest request,long created)
    {
        synchronized(_sessions)
        {
            String id=_useRequestedId?request.getRequestedSessionId():null;
            while (id==null || id.length()==0 || _sessions.containsKey(id))
            {
                long r = _random.nextLong();
                if (r<0)r=-r;
                id=Long.toString(r,30+(int)(created%7));
                String worker = (String)request.getAttribute("org.mortbay.http.ajp.JVMRoute");
                if (worker!=null)
                    id+="."+worker;
                else if (_workerName!=null)
                    id+="."+_workerName;
            }
            return id;
        }
    }
    
    /* ------------------------------------------------------------ */
    public HttpSession getHttpSession(String id)
    {
        synchronized(_sessions)
        {
            return (HttpSession)_sessions.get(id);
        }
    }

    /* ------------------------------------------------------------ */
    public HttpSession newHttpSession(HttpServletRequest request)
    {
        Session session = newSession(request);
        session.setMaxInactiveInterval(_dftMaxIdleSecs);
        synchronized(_sessions)
        {
            _sessions.put(session.getId(),session);
            if (_sessions.size () > this._maxSessions)
               this._maxSessions = _sessions.size ();
        }
        
        HttpSessionEvent event=new HttpSessionEvent(session);
        
        for(int i=0;i<_sessionListeners.size();i++)
            ((HttpSessionListener)_sessionListeners.get(i))
                .sessionCreated(event);
        return session;
    }
    

    /* ------------------------------------------------------------ */
    protected abstract Session newSession(HttpServletRequest request);
    
    /* ------------------------------------------------------------ */
    /** Get the workname.
     * If set, the workername is dot appended to the session ID
     * and can be used to assist session affinity in a load balancer.
     * @return String or null
     */
    public String getWorkerName()
    {
        return _workerName;
    }

    /* ------------------------------------------------------------ */
    /** Set the workname.
     * If set, the workername is dot appended to the session ID
     * and can be used to assist session affinity in a load balancer.
     * @param workerName 
     */
    public void setWorkerName(String workerName)
    {
        _workerName = workerName;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return seconds 
     */
    public int getMaxInactiveInterval()
    {
        return _dftMaxIdleSecs;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param seconds 
     */
    public void setMaxInactiveInterval(int seconds)
    {
        _dftMaxIdleSecs = seconds;
        if (_dftMaxIdleSecs>0 && _scavengePeriodMs>_dftMaxIdleSecs*100)
            setScavengePeriod((_dftMaxIdleSecs+9)/10);
    }
    
    
    /* ------------------------------------------------------------ */
    /** 
     * @return seconds 
     */
    public int getScavengePeriod()
    {
        return _scavengePeriodMs/1000;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param seconds 
     */
    public void setScavengePeriod(int seconds)
    {
        if (seconds==0)
            seconds=60;
        
        int old_period=_scavengePeriodMs;
        int period = seconds*1000;
        if (period>60000)
            period=60000;
        if (period<1000)
            period=1000;
        
        if (period!=old_period)
        {
            synchronized(this)
            {
                _scavengePeriodMs=period;
                if (_scavenger!=null)
                    _scavenger.interrupt();
            }
        }
    }
    
    
    /* ------------------------------------------------------------ */
    public void addEventListener(EventListener listener)
        throws IllegalArgumentException
    {
        boolean known =false;
        if (listener instanceof HttpSessionAttributeListener)
        {
            _sessionAttributeListeners.add(listener);
            known=true;
        }
        if (listener instanceof HttpSessionListener)
        {
            _sessionListeners.add(listener);
            known=true;
        }

        if (!known)
            throw new IllegalArgumentException("Unknown listener "+listener);
    }
    
    /* ------------------------------------------------------------ */
    public void removeEventListener(EventListener listener)
    {
        if (listener instanceof HttpSessionAttributeListener)
            _sessionAttributeListeners.remove(listener);
        if (listener instanceof HttpSessionListener)
            _sessionListeners.remove(listener);
    }
    
    /* ------------------------------------------------------------ */
    public boolean isStarted()
    {
        return _scavenger!=null;
    }
    
    /* ------------------------------------------------------------ */
    public void start()
        throws Exception
    {
        if (_random==null)
        {
            Code.debug("New random session seed");
            _random=new Random();
        }
        else
            Code.debug("Initializing random session key: ",_random);
        _random.nextLong();
        
        if (_sessions==null)
            _sessions=new HashMap();
        
        // Start the session scavenger if we haven't already
        if (_scavenger == null)
        {
            _scavenger = new SessionScavenger();
            _scavenger.start();
        }
    }
    
    
    /* ------------------------------------------------------------ */
    public void stop()
    {
        // Invalidate all sessions to cause unbind events
        ArrayList sessions = new ArrayList(_sessions.values());
        for (Iterator i = sessions.iterator(); i.hasNext(); )
        {
            Session session = (Session)i.next();
            session.invalidate();
        }
        _sessions.clear();
        
        // stop the scavenger
        SessionScavenger scavenger = _scavenger;
        _scavenger=null;
        if (scavenger!=null)
            scavenger.interrupt();
    }
    
    /* -------------------------------------------------------------- */
    /** Find sessions that have timed out and invalidate them.
     *  This runs in the SessionScavenger thread.
     */
    private void scavenge()
    {
        Thread thread = Thread.currentThread();
        ClassLoader old_loader = thread.getContextClassLoader();
        try
        {
	    if (_handler==null)
		return;

            ClassLoader loader = _handler.getClassLoader();
            if (loader!=null)
                thread.setContextClassLoader(loader);
            
            long now = System.currentTimeMillis();
            
            // Since Hashtable enumeration is not safe over deletes,
            // we build a list of stale sessions, then go back and invalidate them
            Object stale=null;
            
            // For each session
            try
            {
                for (Iterator i = _sessions.values().iterator(); i.hasNext(); )
                {
                    Session session = (Session)i.next();
                    long idleTime = session._maxIdleMs;
                    if (idleTime > 0 && session._accessed + idleTime < now) {
                        // Found a stale session, add it to the list
                        stale=LazyList.add(stale,session);
                    }
                }
            }
            catch(ConcurrentModificationException e)
            {
                Code.ignore(e);
                // Oops something changed while we were looking.
                // Lock the context and try again.
                // Set our priority high while we have the sessions locked
                int oldPriority = Thread.currentThread().getPriority();
                Thread.currentThread().setPriority(Thread.MAX_PRIORITY);
                try
                {
                    synchronized(this)
                    {
                        stale=null;
                        scavenge();
                    }
                }
                finally {Thread.currentThread().setPriority(oldPriority);}
            }

            // Remove the stale sessions
            for (int i = LazyList.size(stale); i-->0;)
            {
                ((Session)LazyList.get(stale,i)).invalidate();
               int nbsess = this._sessions.size();
               if (nbsess < this._minSessions)
                  this._minSessions = nbsess;
            }
        }
        finally
        {
            thread.setContextClassLoader(old_loader);
        }
    }
    

    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* -------------------------------------------------------------- */
    /** SessionScavenger is a background thread that kills off old sessions */
    class SessionScavenger extends Thread
    {
        public void run()
        {
            int period=-1;
            try{
                while (isStarted())
                {
                    try {
                        if (period!=_scavengePeriodMs)
                        {
                            Code.debug("Session scavenger period = "+_scavengePeriodMs/1000+"s");
                            period=_scavengePeriodMs;
                        }
                        sleep(period>1000?period:1000);
                        AbstractSessionManager.this.scavenge();
                    }
                    catch (InterruptedException ex){continue;}
                    catch (Error e) {Code.warning(e);}
                    catch (Exception e) {Code.warning(e);}
                }
            }
            finally
            {
                AbstractSessionManager.this._scavenger=null;
                Code.debug("Session scavenger exited");
            }
        }

        SessionScavenger()
        {
            super("SessionScavenger");
            setDaemon(true);
        }

    }   // SessionScavenger


    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    public abstract class Session implements SessionManager.Session
    {
        Map _values;
        boolean _invalid=false;
        boolean _newSession=true;
        long _created=System.currentTimeMillis();
        long _accessed=_created;
        long _maxIdleMs = _dftMaxIdleSecs*1000;
        String _id;

        /* ------------------------------------------------------------- */
        protected Session(HttpServletRequest request)
        {
            _id=newSessionId(request,_created);
            if (_dftMaxIdleSecs>=0)
                _maxIdleMs=_dftMaxIdleSecs*1000;
        }

        /* ------------------------------------------------------------ */
        protected abstract Map newAttributeMap();

        /* ------------------------------------------------------------ */
        public void access()
        {
            _newSession=false;
            _accessed=System.currentTimeMillis();
        }

        /* ------------------------------------------------------------ */
        public boolean isValid()
        {
            return !_invalid;
        }
        
        /* ------------------------------------------------------------ */
        public ServletContext getServletContext()
        {
            return _handler.getServletContext();
        }
        
        /* ------------------------------------------------------------- */
        public String getId()
            throws IllegalStateException
        {
            return _id;
        }

        /* ------------------------------------------------------------- */
        public long getCreationTime()
            throws IllegalStateException
        {
            if (_invalid) throw new IllegalStateException();
            return _created;
        }

        /* ------------------------------------------------------------- */
        public long getLastAccessedTime()
            throws IllegalStateException
        {
            if (_invalid) throw new IllegalStateException();
            return _accessed;
        }

        /* ------------------------------------------------------------- */
        public int getMaxInactiveInterval()
        {
            if (_invalid) throw new IllegalStateException();
            return (int)(_maxIdleMs / 1000);
        }

        /* ------------------------------------------------------------- */
        /**
         * @deprecated
         */
        public HttpSessionContext getSessionContext()
            throws IllegalStateException
        {
            if (_invalid) throw new IllegalStateException();
            return SessionContext.NULL_IMPL;
        }

        /* ------------------------------------------------------------- */
        public void setMaxInactiveInterval(int secs)
        {
            _maxIdleMs = (long)secs * 1000;
            if (_maxIdleMs>0 && (_maxIdleMs/10)<_scavengePeriodMs)
                AbstractSessionManager.this.setScavengePeriod((secs+9)/10);
        }

        /* ------------------------------------------------------------- */
        public synchronized void invalidate()
            throws IllegalStateException
        {
            if (_invalid) throw new IllegalStateException();

            
            if(__24SessionDestroyed && _sessionListeners != null)
            {        
                HttpSessionEvent event = new HttpSessionEvent(this);
                for(int i=0;i<_sessionListeners.size();i++)
                    ((HttpSessionListener)_sessionListeners.get(i)).
                        sessionDestroyed(event);
            }
                    
            if (_values!=null)
            {
                Iterator iter = _values.keySet().iterator();
                while (iter.hasNext())
                {
                    String key = (String)iter.next();
                    Object value = _values.get(key);
                    iter.remove();
                    unbindValue(key, value);
                    
                    if (_sessionAttributeListeners.size()>0)
                    {
                        HttpSessionBindingEvent event =
                            new HttpSessionBindingEvent(this,key,value);
                        
                        for(int i=0;i<_sessionAttributeListeners.size();i++)
                        {
                            ((HttpSessionAttributeListener)
                             _sessionAttributeListeners.get(i))
                                .attributeRemoved(event);
                        }
                    }
                }
            }
            
            synchronized (AbstractSessionManager.this)
            {
                _invalid=true;
                _sessions.remove(_id);
                
                if(!__24SessionDestroyed && _sessionListeners != null)
                {
                    HttpSessionEvent event = new HttpSessionEvent(this);
                    for(int i=0;i<_sessionListeners.size();i++)
                        ((HttpSessionListener)_sessionListeners.get(i)).
                            sessionDestroyed(event);       
                }
            } 
        }

        /* ------------------------------------------------------------- */
        public boolean isNew()
            throws IllegalStateException
        {
            if (_invalid) throw new IllegalStateException();
            return _newSession;
        }


        /* ------------------------------------------------------------ */
        public synchronized Object getAttribute(String name)
        {
            if (_invalid) throw new IllegalStateException();
            if (_values==null)
                return null;
            return _values.get(name);
        }

        /* ------------------------------------------------------------ */
        public synchronized Enumeration getAttributeNames()
        {
            if (_invalid) throw new IllegalStateException();
            List names = _values==null?Collections.EMPTY_LIST:new ArrayList(_values.keySet());
	    return Collections.enumeration(names);
        }

        /* ------------------------------------------------------------ */
        public synchronized void setAttribute(String name, Object value)
        {
            if (_invalid) throw new IllegalStateException();
            if (_values==null)
                _values=newAttributeMap();
            Object oldValue = _values.put(name,value);

            if (value==null || !value.equals(oldValue))
            {
                unbindValue(name, oldValue);
                bindValue(name, value);
                
                if (_sessionAttributeListeners.size()>0)
                {
                    HttpSessionBindingEvent event =
                        new HttpSessionBindingEvent(this,name,
                                                    oldValue==null?value:oldValue);
                    
                    for(int i=0;i<_sessionAttributeListeners.size();i++)
                    {
                        HttpSessionAttributeListener l =
                            (HttpSessionAttributeListener)
                            _sessionAttributeListeners.get(i);
                        
                        if (oldValue==null)
                            l.attributeAdded(event);
                        else if (value==null)
                            l.attributeRemoved(event);
                        else
                            l.attributeReplaced(event);
                    }
                }
            }
        }

        /* ------------------------------------------------------------ */
        public synchronized void removeAttribute(String name)
        {
            if (_invalid) throw new IllegalStateException();
            if (_values==null)
                return;
            
            Object old=_values.remove(name);
            if (old!=null)
            {
                unbindValue(name, old);
                if (_sessionAttributeListeners.size()>0)
                {
                    HttpSessionBindingEvent event =
                        new HttpSessionBindingEvent(this,name,old);
                    
                    for(int i=0;i<_sessionAttributeListeners.size();i++)
                    {
                        HttpSessionAttributeListener l =
                            (HttpSessionAttributeListener)
                            _sessionAttributeListeners.get(i);
                        l.attributeRemoved(event);
                    }
                }
            }
        }

        /* ------------------------------------------------------------- */
        /**
         * @deprecated 	As of Version 2.2, this method is
         * 		replaced by {@link #getAttribute}
         */
        public Object getValue(String name)
            throws IllegalStateException
        {
            return getAttribute(name);
        }

        /* ------------------------------------------------------------- */
        /**
         * @deprecated 	As of Version 2.2, this method is
         * 		replaced by {@link #getAttributeNames}
         */
        public synchronized String[] getValueNames()
            throws IllegalStateException
        {
            if (_invalid) throw new IllegalStateException();
            if (_values==null)
                return new String[0];
            String[] a = new String[_values.size()];
            return (String[])_values.keySet().toArray(a);
        }

        /* ------------------------------------------------------------- */
        /**
         * @deprecated 	As of Version 2.2, this method is
         * 		replaced by {@link #setAttribute}
         */
        public void putValue(java.lang.String name,
                             java.lang.Object value)
            throws IllegalStateException
        {
            setAttribute(name,value);
        }

        /* ------------------------------------------------------------- */
        /**
         * @deprecated 	As of Version 2.2, this method is
         * 		replaced by {@link #removeAttribute}
         */
        public void removeValue(java.lang.String name)
            throws IllegalStateException
        {
            removeAttribute(name);
        }

        /* ------------------------------------------------------------- */
        /** If value implements HttpSessionBindingListener, call valueBound() */
        private void bindValue(java.lang.String name, Object value)
        {
            if (value!=null && value instanceof HttpSessionBindingListener)
                ((HttpSessionBindingListener)value)
                    .valueBound(new HttpSessionBindingEvent(this,name));            
        }

        /* ------------------------------------------------------------- */
        /** If value implements HttpSessionBindingListener, call valueUnbound() */
        private void unbindValue(java.lang.String name, Object value)
        {
            if (value!=null && value instanceof HttpSessionBindingListener)
                ((HttpSessionBindingListener)value)
                    .valueUnbound(new HttpSessionBindingEvent(this,name));
        }
    }
}
