/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

// Standard Java Packages
import java.util.Iterator;
import java.util.Set;

/**
 * Base class for CorrectiveActions
 *
 * @author Stacy Curl
 */
abstract public class BaseCorrectiveAction
    implements CorrectiveAction
{
    /**
     * Default Constructor for BaseCorrectiveAction
     */
    public BaseCorrectiveAction()
    {
        m_numberOfTimesToApply = 1;
        m_numberOfTimesFailed = 0;
    }

    /**
     * Sets the CorrectiveActionContext of this CorrectiveAction
     *
     * @param    correctiveActionContext the CorrectiveActionContext that this Corrective should use.
     *
     * @return this
     */
    final public CorrectiveAction setCorrectiveActionContext(CorrectiveActionContext correctiveActionContext)
    {
        m_correctiveActionContext = correctiveActionContext;

        return this;
    }

    /**
     * Returns the CorrectiveActionContext of this CorrectiveAction
     *
     * @return the CorrectiveActionContext of this CorrectiveAction
     */
    final public CorrectiveActionContext getCorrectiveActionContext()
    {
        return m_correctiveActionContext;
    }

    /**
     * Sets the number of times this CorrectiveAction can be applied
     *
     * @param    numberOfTimesToApply
     *
     * @return this
     */
    final public CorrectiveAction setNumberOfTimesToApply(final int numberOfTimesToApply)
    {
        m_numberOfTimesToApply = numberOfTimesToApply;

        return this;
    }

    /**
     * Get the total number of times this CorrectiveAction can be applied.
     *
     * @return the total number of times this CorrectiveAction can be applied.
     */
    final public int getNumberOfTimesToApply()
    {
        return m_numberOfTimesToApply;
    }

    /**
     * Determine if any of the CorrectiveActions in <code>correctiveActions</code> overides this one.
     *
     * @param    correctiveActions the Set of CorrectiveActions to compare with this one.
     *
     * @return true if any of the CorrectiveActions in <code>correctiveActions</code> overides this one.
     */
    final public boolean isOverridenBy(final Set correctiveActions)
    {
        boolean isOverriden = false;

        for(Iterator iterator = correctiveActions.iterator(); iterator.hasNext(); )
        {
            CorrectiveAction correctiveAction = (CorrectiveAction) iterator.next();

            if(isOverridenBy(correctiveAction))
            {
                isOverriden = true;

                break;
            }
        }

        return isOverriden;
    }

    /**
     * Apply this CorrectiveAction
     *
     * @return the result of applying this CorrectiveAction
     * @throws CorrectiveActionException if this CorrectiveAction cannot be applied i.e. {@link #canApply()} == false
     * @throws Exception
     */
    final public boolean apply() throws CorrectiveActionException, Exception
    {
        if(!canApply())
        {
            throw new CorrectiveActionException("Cannot apply CorrectiveAction, failed too often");
        }

        try
        {
            boolean result = applyImpl();
            if(!result)
            {
                ++m_numberOfTimesFailed;
            }
            else
            {
                m_numberOfTimesFailed = 0;
            }

            return result;
        }
        catch(Exception e)
        {
            ++m_numberOfTimesFailed;

            throw e;
        }
    }

    /**
     * Returns whether this CorrectiveAction can be applied.
     *
     * @return whether this CorrectiveAction can be applied, i.e. it hasn't failed too much
     */
    final public boolean canApply()
    {
        return (m_numberOfTimesFailed < m_numberOfTimesToApply);
    }

    /**
     * Returns a clone of this CorrectiveAction
     *
     * @return a clone of this CorrectiveAction
     * @throws CloneNotSupportedException
     */
    public Object clone() throws CloneNotSupportedException
    {
        return super.clone();
    }

    /**
     * Actual implementation of the CorrectiveAction, defered to derived classes.
     *
     * @return true if the CorrectiveAction succeeded
     * @throws Exception
     */
    abstract protected boolean applyImpl() throws Exception;

    /** The number of times to apply this CorrectiveAction */
    private int m_numberOfTimesToApply;
    /** The number of consecutive times this CorrectiveAction has failed */
    private int m_numberOfTimesFailed;
    /** The CorrectiveActionContext of this CorrectiveAction */
    private CorrectiveActionContext m_correctiveActionContext;
}
