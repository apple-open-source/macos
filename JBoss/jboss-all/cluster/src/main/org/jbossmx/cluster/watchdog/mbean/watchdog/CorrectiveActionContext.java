/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

// Standard Java Classes
import java.util.HashMap;
import java.util.Map;

/**
 * A Class for storing Contextual objects for CorrectiveActions.
 *
 * @author Stacy Curl
 */
public class CorrectiveActionContext
{
    /**
     * Default constructor for CorrectiveActionContext
     */
    public CorrectiveActionContext()
    {
        m_correctiveActionContext = new HashMap();
        m_delegateCorrectiveActionContext = null;
        m_allowWriteToDelegateCorrectiveActionContext = false;
    }

    /**
     * Constructor for CorrectiveActionContext
     *
     * @param    delegateCorrectiveActionContext the CorrectiveActionContext to delegate operations
     *           to if this CorrectiveActionContext cannot fulfill them.
     * @param    allowWriteToDelegateCorrectiveActionContext whether the delegated
     *           CorrectiveActionContext can be writen to.
     */
    public CorrectiveActionContext(CorrectiveActionContext delegateCorrectiveActionContext,
                                   boolean allowWriteToDelegateCorrectiveActionContext)
    {
        m_delegateCorrectiveActionContext = delegateCorrectiveActionContext;
        m_allowWriteToDelegateCorrectiveActionContext = allowWriteToDelegateCorrectiveActionContext;
    }

    /**
     * Add a context object.
     *
     * @param    key the key to be placed into this context
     * @param    value the value to be placed into this context
     *
     * @return the old object associated with <code>key</code>
     * @throws CorrectiveActionContextException if only the delegate CorrectiveActionContext
     * contains <code>key</code> and if the delegate CorrectiveActionContext cannot be writen to.
     */
    public synchronized Object addContextObject(Object key, Object value)
        throws CorrectiveActionContextException
    {
        Object oldObject = null;

        // If there is no delegate corrective action context, of if there is no object matching
        // 'key' in the delegate then return whatever is in the local map.
        if((getDelegateCorrectiveActionContext() == null)
            || (getDelegateCorrectiveActionContext().getContextObject(key) == null))
        {
            oldObject = m_correctiveActionContext.put(key, value);
        }
        else if(getAllowWriteToDelegateCorrectiveActionContext())
        {
            oldObject = getDelegateCorrectiveActionContext().addContextObject(key, value);
        }
        else
        {
            throw new CorrectiveActionContextException(
                "Cannot overwrite object in delegate with this key: " + key.toString());
        }

        return oldObject;
    }

    /**
     * Gets a context object.
     *
     * @param    key the key of the context object.
     *
     * @return the context object.
     */
    public synchronized Object getContextObject(Object key)
    {
        Object object = m_correctiveActionContext.get(key);

        if((object == null) && (getDelegateCorrectiveActionContext() != null))
        {
            object = getDelegateCorrectiveActionContext().getContextObject(key);
        }

        return object;
    }

    /**
     * Retrieves the delegate CorrectiveActionContext.
     *
     * @return the delegate CorrectiveActionContext.
     */
    private CorrectiveActionContext getDelegateCorrectiveActionContext()
    {
        return m_delegateCorrectiveActionContext;
    }

    /**
     * Determines whether delegate CorrectiveActionContext can be writen to.
     *
     * @return whether delegate CorrectiveActionContext can be writen to.
     */
    private boolean getAllowWriteToDelegateCorrectiveActionContext()
    {
        return m_allowWriteToDelegateCorrectiveActionContext;
    }

    /** The Context storage */
    private Map m_correctiveActionContext;
    /** The delegate CorrectiveActionContext */
    private CorrectiveActionContext m_delegateCorrectiveActionContext;
    /** Whether the delegate CorrectiveActionContext can be writen to */
    private boolean m_allowWriteToDelegateCorrectiveActionContext;
}
