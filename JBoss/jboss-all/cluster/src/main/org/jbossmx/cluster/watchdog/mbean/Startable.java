/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

import org.jboss.logging.Logger;

/**
 * Base class for MBeans that can be monitored
 *
 * @author Stacy Curl
 */
abstract public class Startable
    implements StartableMBean
{
    /**
     * Constructor for Startable
     */
    public Startable()
    {
        setMBeanProvisionalState(STOPPED);
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> StartableMBean methods
    /**
     * Starts the MBean, delegates implementation to the <code>startMBeanImpl</code> method. The
     * MBean will go through several states:
     *
     * <p>(STOPPED | FAILED_TO_START) -> STARTING -> (RUNNING | FAILED_TO_START)
     *
     * @return whether the MBean started
     */
    final synchronized public boolean startMBean()
    {
        LOG.debug("startMBean");

        boolean started = false;

        try
        {
            final int mbeanProvisionalState = getMBeanProvisionalState();

            if((mbeanProvisionalState == STOPPED) || (mbeanProvisionalState == FAILED_TO_START))
            {
                setMBeanProvisionalState(STARTING);

                try
                {
                    started = startMBeanImpl();
                }
                catch(Exception e)
                {
                    LOG.warning("startMBean, Exception thrown");
                    LOG.warning(e);

                    started = false;
                }
                catch(Throwable t)
                {
                    LOG.warning("startMBean, Throwable thrown");
                    LOG.warning(t);

                    started = false;
                }

                setMBeanProvisionalState(started
                                         ? RUNNING
                                         : FAILED_TO_START);
            }

            return started;
        }
        finally
        {
            LOG.debug("startMBean - done = " + started);
        }
    }

    /**
     * Stops the MBean, delegates implementation to the <code>stopMBeanImpl</code> method. The MBean
     * will go through several states:
     *
     * <p> (RUNNING | FAILED_TO_STOP) -> STOPPING -> (STOPPED | FAILED_TO_STOP)
     *
     * @return whether the MBean stopped.
     */
    final synchronized public boolean stopMBean()
    {
        LOG.debug("stopMBean");

        boolean stopped = false;

        try
        {
            final int mbeanProvisionalState = getMBeanProvisionalState();

            if((mbeanProvisionalState == RUNNING) || (mbeanProvisionalState == FAILED_TO_STOP))
            {
                setMBeanProvisionalState(STOPPING);

                try
                {
                    stopped = stopMBeanImpl();
                }
                catch(Exception e)
                {
                    LOG.warning("stopMBean, Exception thrown");
                    LOG.warning(e);

                    stopped = false;
                }
                catch(Throwable t)
                {
                    LOG.warning("startMBean, Throwable thrown");
                    LOG.warning(t);

                    stopped = false;
                }

                setMBeanProvisionalState(stopped
                                         ? STOPPED
                                         : FAILED_TO_STOP);
            }

            return stopped;
        }
        finally
        {
            LOG.debug("stopMBean - done = " + stopped);
        }
    }

    /**
     * Restart an MBean, delegates implementation to <code>restartMBeanImpl</code>. The MBean will
     * go through several states:
     *
     * <p> FAILED -> RESTARTING -> (RUNNING | FAILED)
     *
     * @return
     */
    final synchronized public boolean restartMBean()
    {
        LOG.debug("restartMBean");

        boolean restarted = false;

        try
        {
            final int mbeanState = retrieveMBeanState();

//            final int mbeanState = getMBeanState();

            if(mbeanState == RUNNING)
            {
                restarted = true;
            }
            else if(mbeanState == FAILED)
            {
                setMBeanProvisionalState(RESTARTING);

                try
                {
                    restarted = restartMBeanImpl();
                }
                catch(Exception e)
                {
                    LOG.warning("restartMBean, Exception thrown");
                    LOG.warning(e);

                    restarted = false;
                }
                catch(Throwable t)
                {
                    LOG.warning("startMBean, Throwable thrown");
                    LOG.warning(t);

                    restarted = false;
                }

                setMBeanProvisionalState(RUNNING);
            }

            return restarted;
        }
        finally
        {
            LOG.debug("restartMBean - done = " + restarted);
        }
    }

    /**
     * Gets the state of the MBean
     *
     * @return the state of the MBean
     */
    final synchronized public int retrieveMBeanState()
    {
//        LOG.debug("getMBeanState");

        int actualState;

        try
        {
            int provisionalState;

            provisionalState = getMBeanProvisionalState();

            if((provisionalState == RUNNING) || (provisionalState == FAILED_TO_STOP))
            {
                try
                {
                    actualState = hasMBeanFailed()
                                  ? FAILED
                                  : RUNNING;
                }
                catch(Exception e)
                {
                    LOG.debug("retrieveMBeanState, Exception thrown");
                    LOG.debug(e);

//                    LOG.debug("getMBeanState, Exception thrown", e);
                    actualState = FAILED;
                }
                catch(Throwable t)
                {
                    LOG.warning("retrieveMBeanState, Throwable thrown");
                    LOG.warning(t);

//                    LOG.warning("getMBeanState, Throwable thrown", t);
                    actualState = FAILED;
                }
            }
            else
            {
                actualState = provisionalState;
            }

//            setMBeanStateString(getStateAsString(actualState));

            return actualState;
        }
        finally
        {

//            LOG.debug("getMBeanState - done");
        }
    }

    /**
     * Gets the state of the MBean as a String
     *
     * @return the state of the MBean as a String
     */
    final public String getMBeanStateString()
    {
        return getStateAsString(retrieveMBeanState());
    }

    /**
     * Simulates failure in the MBean.
     */
    final synchronized public void simulateFailure()
    {
        setMBeanProvisionalState(FAILED);
    }

    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< StartableMBean methods

    public String retrieveOneLiner(final String defaultString)
    {
//        final int state = retrieveMBeanState();
          return defaultString + " --- " + getMBeanStateString();
//        if (state == this.FAILED)
//        {
//
//        }
//        else
//        {
//            return defaultString + "
//        }
    }

    /**
     * Sets the provisional state of the MBean
     *
     * @param    provisionalState the provisional state of the MBean
     */
    final private void setMBeanProvisionalState(int provisionalState)
    {
        final String provisionalStateAsString = getStateAsString(provisionalState);
        LOG.debug("setMBeanProvisionalState(" + provisionalStateAsString + ")");

        m_provisionalState = provisionalState;

//        setMBeanStateString(provisionalStateAsString);
    }

    /**
     * Gets the provisional state of the MBean
     *
     * @return the provisional state of the MBean
     */
    final private int getMBeanProvisionalState()
    {
        return m_provisionalState;
    }

    /**
     * Converts <code>state</code> to a String
     *
     * @param    state the MBean state to convert
     *
     * @return <code>state</code> converted to a String
     */
    final public static String getStateAsString(final int state)
    {
        return m_sStateStrings[state + 3];

//        String stateAsString = "";
//
//        switch(state)
//        {
//            case STOPPED:
//                stateAsString = STOPPED_STRING;
//                break;
//
//            case STARTING:
//                stateAsString = STARTING_STRING;
//                break;
//
//            case RUNNING:
//                stateAsString = RUNNING_STRING;
//                break;
//
//            case STOPPING:
//                stateAsString = STOPPING_STRING;
//                break;
//
//            case RESTARTING:
//                stateAsString = RESTARTING_STRING;
//                break;
//
//            case FAILED:
//                stateAsString = FAILED_STRING;
//                break;
//
//            case FAILED_TO_START:
//                stateAsString = FAILED_TO_START_STRING;
//                break;
//
//            case FAILED_TO_STOP:
//                stateAsString = FAILED_TO_STOP_STRING;
//                break;
//
//            default:
//                break;
//        }
//
//        return stateAsString;
    }

    /**
     *
     * @return
     * @throws Exception
     */
    abstract protected boolean startMBeanImpl()
        throws Exception;

    /**
     *
     * @return
     * @throws Exception
     */
    abstract protected boolean stopMBeanImpl()
        throws Exception;

    /**
     *
     * @return
     * @throws Exception
     */
    abstract protected boolean restartMBeanImpl()
        throws Exception;

    /**
     *
     * @return
     * @throws Exception
     */
    abstract protected boolean hasMBeanFailed()
        throws Exception;

    /** The provisional state of the MBean */
    private int m_provisionalState;

    /** A Failed to Stop MBean State */
    public static final int FAILED_TO_STOP = -3;
    /** A Failed to Start MBean State */
    public static final int FAILED_TO_START = -2;
    /** A Failed MBean State */
    public static final int FAILED = -1;
    /** A Stopped MBean State */
    public static final int STOPPED = 0;
    /** A Starting MBean State */
    public static final int STARTING = 1;
    /** A Running MBean State */
    public static final int RUNNING = 2;
    /** A Stopping MBean State */
    public static final int STOPPING = 3;
    /** A Restarting MBean State */
    public static final int RESTARTING = 4;

    /** The String representation of the states */
    public static final String[] m_sStateStrings = {"FAILED TO STOP", "FAILED TO START", "FAILED",
        "STOPPED", "STARTING", "RUNNING", "STOPPING", "RESTARTING"};

//    /** */
//    public static final String FAILED_STRING = "FAILED";
//    /** */
//    public static final String FAILED_TO_START_STRING = "FAILED TO START";
//    /** */
//    public static final String FAILED_TO_STOP_STRING = "FAILED TO STOP";
//    /** */
//    public static final String STOPPED_STRING = "STOPPED";
//    /** */
//    public static final String STARTING_STRING = "STARTING";
//    /** */
//    public static final String RUNNING_STRING = "RUNNING";
//    /** */
//    public static final String STOPPING_STRING = "STOPPING";
//    /** */
//    public static final String RESTARTING_STRING = "RESTARTING";

    private static Log LOG = Log.createLog(Startable.class.getName());
}
