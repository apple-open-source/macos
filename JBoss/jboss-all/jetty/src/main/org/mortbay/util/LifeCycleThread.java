package org.mortbay.util;

import java.io.InterruptedIOException;

/* ------------------------------------------------------------ */
/** Base Thread class implementing LifeCycle.
 *
 * @version $Revision: 1.4.4.3 $
 * @author Greg Wilkins (gregw)
 */
public abstract class LifeCycleThread implements LifeCycle, Runnable
{
    private boolean _running;
    private boolean _daemon ;
    private Thread _thread;
    
    /* ------------------------------------------------------------ */
    public boolean isDaemon()
    {
        return _daemon;
    }
    
    /* ------------------------------------------------------------ */
    public void setDaemon(boolean d)
    {
        _daemon = d;
    }
    
    /* ------------------------------------------------------------ */
    public Thread getThread()
    {
        return _thread;
    }
    
    /* ------------------------------------------------------------ */
    public boolean isStarted()
    {
        return _running;
    }

    /* ------------------------------------------------------------ */
    public synchronized void start()
        throws Exception
    {
        if (_running)
        {
            Code.debug("Already started");
            return;
        }
        _running=true;
        if (_thread==null)
        {
            _thread=new Thread(this);
            _thread.setDaemon(_daemon);
        }
        _thread.start();
    }
    
    /* ------------------------------------------------------------ */
    /** 
     */
    public synchronized void stop()
        throws InterruptedException
    {
        _running=false;
        if (_thread!=null)
        {
            _thread.interrupt();
            _thread.join();
        }
    }
    

    /* ------------------------------------------------------------ */
    /** 
     */
    public final void run()
    {
        try
        {
            while(_running)
            {
                try
                {
                    loop();
                }
                catch(InterruptedException e)
                {
                    if (Code.verbose())
                        Code.ignore(e);
                }
                catch(InterruptedIOException e)
                {
                    if (Code.verbose())
                        Code.ignore(e);
                }
                catch(Exception e)
                {
                    if (exception(e))
                        break;
                } 
                catch(Error e)
                {
                    if (error(e))
                        break;
                }   
            }
        }
        finally
        {
            _running=false;
        }
    }

    /* ------------------------------------------------------------ */
    /** Handle exception from loop.
     * @param e The exception
     * @return True of the loop should continue;
     */
    public boolean exception(Exception e)
    {
        Code.warning(e);
        return true;
    }
    
    /* ------------------------------------------------------------ */
    /** Handle error from loop.
     * @param e The exception
     * @return True of the loop should continue;
     */
    public boolean error(Error e)
    {
        Code.warning(e);
        return true;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @exception InterruptedException 
     * @exception InterruptedIOException 
     */
    public abstract void loop()
    throws InterruptedException,
           InterruptedIOException,
           Exception;
    
}
