/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

import org.jboss.logging.Log;

/**
 * A Test MBean
 *
 * @author Stacy Curl
 */
public class Test1
    extends Startable
    implements Test1MBean
{
    /**
     * Constructor for Test1
     */
    public Test1()
        throws Exception
    {
        throw new Exception("Mwahhahhahahahahahahahhhahahahahahahahahahahahahhhahahhahhahahahaaaa");

//        try
//        {
//            setIsRunning(false);
//            setCallSuccess(true);
//            setCallDelay(0);
//        }
//        catch (Throwable t)
//        {
//            t.printStackTrace();
//        }
    }

    /**
     * Starts Test1, will succeed if <code>getCallSuccess</code> is true. Will take at least
     * <code>getCallDelay</code> milliseconds to complete.
     *
     * @return whether Test1 was 'started'
     */
    protected boolean startMBeanImpl()
    {
        LOG.debug("startMBeanImpl");
        sleep(getCallDelay());

        if(getCallSuccess())
        {
            setIsRunning(true);
        }

        LOG.debug("startMBeanImpl - done");

        return getCallSuccess();
    }

    /**
     * Stops Test1, will succeed if <code>getCallSuccess</code> is true. Will take at least
     * <code>getCallDelay</code> milliseconds to complete.
     *
     * @return whether Test1 was 'stopped'
     */
    protected boolean stopMBeanImpl()
    {
        sleep(getCallDelay());

        if(getCallSuccess())
        {
            setIsRunning(false);
        }

        return getCallSuccess();
    }

    /**
     * Delegates to <code>startMBeanImpl</code>
     *
     * @return result from <code>startMBeanImpl</code>
     */
    protected boolean restartMBeanImpl()
    {
        return startMBeanImpl();
    }

    /**
     * Returns whether Test1 isn't 'running'
     *
     * @return whether Test1 isn't 'running'
     */
    protected boolean hasMBeanFailed()
    {
        return !isMBeanRunning();
    }

    /**
     * Returns whether Test1 is 'running'
     *
     * @return whether Test1 is 'running'
     */
    protected boolean isMBeanRunning()
    {
        return m_isRunning;
    }

    /**
     * Sets whether calls to StartableMBean methods will succeed.
     *
     * @param    callSuccess whether calls to StartableMBean methods will succeed.
     */
    public void setCallSuccess(boolean callSuccess)
    {
        m_callSuccess = callSuccess;
    }

    /**
     * Gets whether calls to StartableMBean methods will succeed.
     *
     * @return whether calls to StartableMBean methods will succeed.
     */
    public boolean getCallSuccess()
    {
        return m_callSuccess;
    }

    /**
     * Sets how long calls to to StartableMBean methods will take.
     *
     * @param    callDelay how long calls to to StartableMBean methods will take.
     */
    public void setCallDelay(long callDelay)
    {
        m_callDelay = callDelay;
    }

    /**
     * Gets how long calls to to StartableMBean methods will take.
     *
     * @return how long calls to to StartableMBean methods will take.
     */
    public long getCallDelay()
    {
        return m_callDelay;
    }

    /**
     * Sets the 'running' state of Test1
     *
     * @param    isRunning
     */
    private void setIsRunning(boolean isRunning)
    {
        LOG.debug("setIsRunning(" + isRunning + ")");

        m_isRunning = isRunning;
    }

    /**
     * Sleep for a while
     *
     * @param    milliseconds the amount if time in milliseconds to sleep.
     */
    private void sleep(long milliseconds)
    {
        if(milliseconds > 0)
        {
            try
            {
                Thread.currentThread().sleep(milliseconds);
            }
            catch(Exception e) {}
        }
    }

    /** The 'running' state of Test1 */
    private boolean m_isRunning;
    /** Whether calls to StartableMBean methods will succeed */
    private boolean m_callSuccess;
    /** How long calls to StartableMBean methods will take */
    private long m_callDelay;

    private static Log LOG = Log.createLog(Test1.class.getName());
}
