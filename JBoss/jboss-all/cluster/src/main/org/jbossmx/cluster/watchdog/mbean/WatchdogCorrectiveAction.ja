/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

import org.jbossmx.cluster.watchdog.HermesException;

/**
 * This class encapsulates the sequences of corrective actions the Watchdog should take
 * when the StartableMBean fails.
 *
 * @author Stacy Curl
 */
public class WatchdogCorrectiveAction
{
    /** Constant identifying an Invoke Restart Method Corrective Action */
    public static final int INVOKE_RESTART_METHOD = 0;
    /** Constant identifying a Reregister MBean Corrective Action */
    public static final int REREGISTER_MBEAN = 1;
    /** Constant identifying a Restart Agent Corrective Action */
    public static final int RESTART_AGENT = 2;
    /** Constant identifying a Restart Machine Corrective Action */
    public static final int RESTART_MACHINE = 3;

//    public static final int MAX_INVOKE_START_METHOD = 2;
//    public static final int MAX_REREGISTER_MBEAN = 2;
//    public static final int MAX_RESTART_AGENT = 2;
//    public static final int MAX_RESTART_MACHINE = 1;

    /**
     * Constructor for WatchdogCorrectiveAction
     *
     * @param    watchdogInterface the Watchdog that is watching the thing this object will correct. mmm
     * @throws HermesException if INVOKE_RESTART_METHOD is an invalid Corrective Action
     * @see validateCorrectiveAction
     */
    public WatchdogCorrectiveAction(WatchdogMBean watchdogInterface) throws HermesException
    {
        this(watchdogInterface, INVOKE_RESTART_METHOD);
    }

    /**
     * Constructor for WatchdogCorrectiveAction
     * @param    watchdogInterface the Watchdog that is watching the thing this object will correct. mmm
     * @param    initialCorrectiveAction the initial Corrective Action to take
     * @throws HermesException if <code>initialCorrectiveAction</code> is an invalid Corrective
     * Action
     * @see validateCorrectiveAction
     */
    public WatchdogCorrectiveAction(WatchdogMBean watchdogInterface, int initialCorrectiveAction)
        throws HermesException
    {
        setWatchdogInterface(watchdogInterface);
        setInitialCorrectiveAction(getNextAvailableCorrectiveAction(initialCorrectiveAction));
        setCurrentCorrectiveAction(getNextAvailableCorrectiveAction(initialCorrectiveAction));
        setWorstCorrectiveAction(getCurrentCorrectiveAction());
    }

    /**
     * Gets the current Corrective Action
     *
     * @return the current Corrective Action
     */
    public int getCurrentCorrectiveAction()
    {
        return m_currentCorrectiveAction;
    }

    /**
     * Gets the worst current CorrectiveAction so far
     *
     * @return the worst current CorrectiveAction so far
     */
    public int getWorstCorrectiveAction()
    {
        return m_worstCorrectiveAction;
    }

    /**
     * Set whether the current Corrective Action succeeded
     *
     * @param    succeeded whether the current Corrective Action succeeded
     * @throws HermesException if the current Corrective Action is updated and the new Corrective
     * Action is invalid
     * @see validateCorrectiveAction
     */
    public void setCorrectiveActionSucceeded(final boolean succeeded) throws HermesException
    {
        if(succeeded)
        {
            updateCorrectiveAction(getInitialCorrectiveAction());
        }
        else if(hasCountReachedMax())
        {
            increaseCorrectiveActionSeverity();
        }
    }

    /**
     * Update the current Corrective Action, also updates the worst current Corrective Action.
     *
     * @param    correctiveAction the new current CorrectiveAction
     * @throws HermesException if the current Corrective Action is updated and the new Corrective
     * Action is invalid
     * @see validateCorrectiveAction
     */
    private void updateCorrectiveAction(final int correctiveAction) throws HermesException
    {
        if(getCurrentCorrectiveAction() != correctiveAction)
        {
            setCount(0);
        }

        setCurrentCorrectiveAction(correctiveAction);
        updateWorstCorrectiveAction();

        setCount(getCount() + 1);
    }

    /**
     * Update the current Corrective Action to point to the next Corrective Action that is
     * applicable.
     *
     * @throws HermesException if the the new Corrective Action is invalid
     * @see validateCorrectiveAction
     */
    private void increaseCorrectiveActionSeverity() throws HermesException
    {
        setCurrentCorrectiveAction(getNextAvailableCorrectiveAction(getCurrentCorrectiveAction()
            + 1));

//        final int nextAvailableCorrectiveAction = getNextAvailableCorrectiveAction(
//            getCurrentCorrectiveAction() + 1);
//
//        if(nextAvailableCorrectiveAction != -1)
//        {
//            setCurrentCorrectiveAction(nextAvailableCorrectiveAction);
//        }
//        else
//        {
//            throw new HermesException(
//                "WatchdogCorrectiveAction.increaseCorrectiveActionSeverity - no more corrective actions available",
//                null);
//        }
    }

    /**
     * Gets the next available Corrective Action that is applicable, returns -1 if there are no
     * Corrective Actions >= <code>startingCorrectiveAction</code>
     *
     * @param    startingCorrectiveAction the Corrective Action to start at
     *
     * @return the next available Corrective Action that is applicable
     */
    private int getNextAvailableCorrectiveAction(final int startingCorrectiveAction)
    {
        int nextAvailableCorrectiveAction = startingCorrectiveAction;

        while((getNumTimesToAttemptCorrectiveAction(nextAvailableCorrectiveAction) == 0)
            && (nextAvailableCorrectiveAction <= RESTART_MACHINE))
        {
            ++nextAvailableCorrectiveAction;
        }

        if(nextAvailableCorrectiveAction > RESTART_MACHINE)
        {
            nextAvailableCorrectiveAction = -1;
        }

        return nextAvailableCorrectiveAction;
    }

//    /**
//     * I don't understand this code, the <code>getNextAvailableCorrectiveAction</code> should be
//     * enough, this method should be deleted.
//     *
//     * @param    initialCorrectiveAction
//     *
//     * @return
//     */
//    private int getFirstApplicableCorrectiveAction(int initialCorrectiveAction)
//    {
//        int currentFirstApplicableCorrectiveAction = initialCorrectiveAction;
//
//        while((getNumTimesToAttemptCorrectiveAction(currentFirstApplicableCorrectiveAction) == 0)
//            && (currentFirstApplicableCorrectiveAction != RESTART_MACHINE))
//        {
//            ++currentFirstApplicableCorrectiveAction;
//        }
//
//        return currentFirstApplicableCorrectiveAction;
//    }

    /**
     * Gets the number of times a Corrective Action can be attempted
     *
     * @param    correctiveAction the Corrective Action
     *
     * @return the number of times a Corrective Action can be attempted
     */
    private int getNumTimesToAttemptCorrectiveAction(final int correctiveAction)
    {
        switch(correctiveAction)
        {
            case INVOKE_RESTART_METHOD:
                return getWatchdogInterface().getNumTimesToAttemptMBeanRestart();

            case REREGISTER_MBEAN:
                return getWatchdogInterface().getNumTimesToAttemptMBeanReregister();

            case RESTART_AGENT:
                return getWatchdogInterface().getNumTimesToAttemptAgentRestart();

            default:    //RESTART_MACHINE:
                return getWatchdogInterface().getNumTimesToAttemptMachineRestart();
        }
    }

    /**
     * Updates the worst corrective action so that it is at least equal to the current corrective
     * action.
     * @throws HermesException if the the new worst Corrective Action is invalid
     * @see validateCorrectiveAction
     */
    private void updateWorstCorrectiveAction() throws HermesException
    {
        if(getCurrentCorrectiveAction() > getWorstCorrectiveAction())
        {
            setWorstCorrectiveAction(getCurrentCorrectiveAction());
        }
    }

    /**
     * Gets whether the current corrective action has reached the maximum number of times it can
     * be attempted.
     *
     * @return whether the current corrective action can be attempted again.
     */
    private boolean hasCountReachedMax()
    {
        return (getCount() >= getNumTimesToAttemptCorrectiveAction(getCurrentCorrectiveAction()));
    }

    /**
     * Gets the WatchdogMBean which is watching the MBean that this Corrective Action applied to.
     *
     * @return the WatchdogMBean which is watching the MBean that this Corrective Action applied to.
     */
    private WatchdogMBean getWatchdogInterface()
    {
        return m_watchdogInterface;
    }

    /**
     * Sets the WatchdogMBean which is watching the MBean that this Corrective Action applied to.
     *
     * @param    watchdogInterface the WatchdogMBean which is watching the MBean that this
     * Corrective Action applied to.
     */
    private void setWatchdogInterface(WatchdogMBean watchdogInterface)
    {
        m_watchdogInterface = watchdogInterface;
    }

    /**
     * Gets the initial Corrective Action
     *
     * @return the initial Corrective Action
     */
    private int getInitialCorrectiveAction()
    {
        return m_initialCorrectiveAction;
    }

    /**
     * Sets the initial Corrective Action
     *
     * @param    initialCorrectiveAction the initial Corrective Action
     * @throws HermesException if <code>initialCorrectiveAction</code> is an invalid Corrective Action
     * @see validateCorrectiveAction
     */
    private void setInitialCorrectiveAction(final int initialCorrectiveAction)
        throws HermesException
    {
        validateCorrectiveAction(initialCorrectiveAction);

        m_initialCorrectiveAction = initialCorrectiveAction;
//        m_initialCorrectiveAction = getFirstApplicableCorrectiveAction(initialCorrectiveAction);
    }

    /**
     * @param    currentCorrectiveAction
     * @throws HermesException if <code>currentCorrectiveAction</code> is an invalid Corrective
     * Action
     * @see validateCorrectiveAction
     */
    private void setCurrentCorrectiveAction(int currentCorrectiveAction) throws HermesException
    {
        validateCorrectiveAction(currentCorrectiveAction);

        m_currentCorrectiveAction = currentCorrectiveAction;
//        m_currentCorrectiveAction = getFirstApplicableCorrectiveAction(currentCorrectiveAction);
    }

    /**
     * @param    worstCorrectiveAction
     * @throws HermesException if <code>worstCorrectiveAction</code> is an invalid Corrective Action
     * @see validateCorrectiveAction
     */
    private void setWorstCorrectiveAction(int worstCorrectiveAction) throws HermesException
    {
        validateCorrectiveAction(worstCorrectiveAction);

        m_worstCorrectiveAction = worstCorrectiveAction;
    }

    /**
     * Validates a Corrective Action. A Corrective Action is valid if it is >= INVOKE_RESTART_METHOD
     * and <= RESTART_MACHINE.
     *
     * @param    correctiveAction the Corrective Action to validate
     * @throws HermesException if <code>correctiveAction</code> is invalid.
     */
    private void validateCorrectiveAction(final int correctiveAction) throws HermesException
    {
        if((correctiveAction < INVOKE_RESTART_METHOD) || (correctiveAction > RESTART_MACHINE))
        {
            throw new HermesException("Invalid Corrective Action", null);
        }
    }

    /**
     * Sets the number of times that the current Corrective Action has been applied
     *
     * @param    count the number of times that the current Corrective Action has been applied
     */
    private void setCount(int count)
    {
        m_count = count;
    }

    /**
     * Gets the number of times that the current Corrective Action has been applied
     *
     * @return the number of times that the current Corrective Action has been applied
     */
    private int getCount()
    {
        return m_count;
    }

    /** The WatchdogMBean which is watching the MBean that this Corrective Action applied to */
    private WatchdogMBean m_watchdogInterface;
    /** The initial Corrective Action */
    private int m_initialCorrectiveAction;
    /** The current Corrective Action */
    private int m_currentCorrectiveAction;
    /** The number of times that the current Corrective Action has been applied */
    private int m_count;
    /** The worst Corrective Action */
    private int m_worstCorrectiveAction;
}
