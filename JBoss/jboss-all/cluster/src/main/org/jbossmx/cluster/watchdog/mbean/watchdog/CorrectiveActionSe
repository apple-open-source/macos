/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

// Standard Java Packages
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * A sequence of CorrectiveActions
 */
public class CorrectiveActionSequence
{
    /** Default constructor for CorrectiveActionSequence
     */
    public CorrectiveActionSequence()
    {
        this(null);
    }

    /**
     * Constructor for CorrectiveActionSequence
     *
     * @param    correctiveActionContext the CorrectiveActonContext that will be used for all
     * CorrectiveActions added to this class.
     */
    public CorrectiveActionSequence(CorrectiveActionContext correctiveActionContext)
    {
        m_correctiveActionSequence = new ArrayList();
        m_currentCorrectiveAction = 0;
        m_correctiveActionContext = correctiveActionContext;
    }

    /**
     * Adds a corrective action.
     *
     * @param   correctiveAction the CorrectiveAction to add.
     */
    public boolean addCorrectiveAction(CorrectiveAction correctiveAction)
    {
        boolean result = m_correctiveActionSequence.add(correctiveAction);

        if((m_correctiveActionContext != null) && result)
        {
            correctiveAction.setCorrectiveActionContext(m_correctiveActionContext);
        }

        return result;
    }

    /**
     * Obtains the current CorrectiveAction to apply and applies it
     *
     * @return the result of applying the CorrectiveAction
     * @throws CorrectiveActionException if there is no CorrectiveAction to apply
     * @throws Exception
     */
    public boolean applyCurrentCorrectiveAction() throws CorrectiveActionException, Exception
    {
        CorrectiveAction currentCorrectiveAction = getCurrentCorrectiveAction();
        if(currentCorrectiveAction == null)
        {
            throw new CorrectiveActionException("No current corrective action");
        }

        return currentCorrectiveAction.apply();
    }

    /**
     * Determines if the next available CorrectiveAction in
     * <code>otherCorrectiveActionSequence</code> overides the next CorrectiveAction this class.
     *
     * @param    otherCorrectiveActionSequence the CorrectiveActionSequence to compare to this class.
     *
     * @return whether the next CorrectiveAction in <code>otherCorrectiveActionSequence</code>
     * overides the next CorrectiveAction in this class.
     */
    public boolean isOverridenBy(final CorrectiveActionSequence otherCorrectiveActionSequence)
    {
        final CorrectiveAction thisCurrectiveAction = getCurrentCorrectiveAction();
        final CorrectiveAction otherCurrectiveAction = otherCorrectiveActionSequence
            .getCurrentCorrectiveAction();

        return (thisCurrectiveAction != null) && (otherCurrectiveAction != null)
               && thisCurrectiveAction.isOverridenBy(otherCurrectiveAction);
    }

    /**
     * Determines whether any of the CorrectiveActionSequences in
     * <code>otherCorrectiveActionSequences</code> overides this class.
     *
     * @param    otherCorrectiveActionSequences the Set of CorrectiveActionSequences to compare
     * with this class.
     *
     * @return whether any of the CorrectiveActionSequences in
     * <code>otherCorrectiveActionSequences</code> overides this class.
     */
    final public boolean isOverridenBy(final Set otherCorrectiveActionSequences)
    {
        boolean isOverriden = false;

        for(Iterator iterator = otherCorrectiveActionSequences.iterator(); iterator.hasNext(); )
        {
            CorrectiveActionSequence correctiveActionSequence = (CorrectiveActionSequence) iterator
                .next();

            if(isOverridenBy(correctiveActionSequence))
            {
                isOverriden = true;

                break;
            }
        }

        return isOverriden;
    }

    /**
     * Obtains the current corrective action, skipping through CorrectiveActions until a
     * corrective action which can be applied is found. If none can be found then it returns null
     *
     * @return the next applicable CorrectiveAction
     */
    private CorrectiveAction getCurrentCorrectiveAction()
    {
        CorrectiveAction currentCorrectiveAction = null;

        if((m_correctiveActionSequence != null) && (m_currentCorrectiveAction != -1))
        {
            for(int cLoop = m_currentCorrectiveAction; cLoop < m_correctiveActionSequence.size();
                ++cLoop)
            {
                CorrectiveAction currentAction = (CorrectiveAction) m_correctiveActionSequence
                    .get(m_currentCorrectiveAction);

                if(currentAction.canApply())
                {
                    currentCorrectiveAction = currentAction;

                    break;
                }
            }
        }

        return currentCorrectiveAction;
    }

    /** The sequence of CorrectiveActions to take */
    private List m_correctiveActionSequence;
    /** The index of the current CorrectiveAction */
    private int m_currentCorrectiveAction;
    /** The CorrectiveActionContext for all CorrectiveActions added to this CorrectiveActionSequence*/
    private CorrectiveActionContext m_correctiveActionContext;
}
