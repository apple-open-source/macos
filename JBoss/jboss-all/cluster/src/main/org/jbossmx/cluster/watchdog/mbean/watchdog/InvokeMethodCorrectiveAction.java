/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

// Standard Java Packages
import java.util.Map;

// Java Extension Packages

import javax.management.MBeanServer;
import javax.management.ObjectName;

/**
 * CorrectiveAction for invoking methods on mbeans
 *
 * @author Stacy Curl
 */
public class InvokeMethodCorrectiveAction
    extends BaseCorrectiveAction
{
    /**
     * Constructor for InvokeMethodCorrectiveAction
     *
     * @param    methodName the method to invoke on the MBean
     * @param    parameters the parameters of <code>methodName</code>
     * @param    signature the signature of <code>methodName</code>
     * @param    desiredResult the results expect from <code>methodName</code> when it succeeds
     */
    public InvokeMethodCorrectiveAction(String methodName, Object[] parameters, String[] signature,
                                        Object desiredResult)
    {
        m_methodName = methodName;
        m_parameters = parameters;
        m_signature = signature;
        m_desiredResult = desiredResult;
    }

    /**
     * Apply the InvokeMethodCorrectiveAction, retrieves the MBeanServer and ObjectName from the
     * CorrectiveActionContext. Then invokes the method specified during construction.
     *
     * @return whether the result returned from the invoked method matches the desired result.
     * @throws Exception
     */
    protected boolean applyImpl() throws Exception
    {
        MBeanServer mbeanServer = (MBeanServer) getCorrectiveActionContext()
            .getContextObject(CorrectiveActionContextConstants.Agent_MBeanServer);
        ObjectName objectName = (ObjectName) getCorrectiveActionContext()
            .getContextObject(CorrectiveActionContextConstants.MBean_ObjectName);

        Object result = mbeanServer.invoke(objectName, m_methodName, m_parameters, m_signature);

        return matchesDesiredResult(result);
    }

    /**
     * Returns whether this CorrectiveAction is overriden by <code>correctiveAction</code>
     *
     * @param    correctiveAction the CorrectiveAction to compare to this one.
     *
     * @return whether this CorrectiveAction is overriden by <code>correctiveAction</code>
     */
    public boolean isOverridenBy(final CorrectiveAction correctiveAction)
    {
        return isOveridingCallScriptCorrectiveAction(correctiveAction)
               || (correctiveAction instanceof RestartAgentCorrectiveAction);
    }

    /**
     * Returns whether <code>correctiveAction</code> is a CallScriptCorrectiveAction which has
     * been set to override InvokeMethodCorrectiveAction
     *
     * @param    correctiveAction the CorrectiveAction to check
     *
     * @return whether <code>correctiveAction</code> is a CallScriptCorrectiveAction which has
     * been set to override InvokeMethodCorrectiveAction
     */
    private boolean isOveridingCallScriptCorrectiveAction(final CorrectiveAction correctiveAction)
    {
        return (correctiveAction instanceof CallScriptCorrectiveAction)
               && ((CallScriptCorrectiveAction) correctiveAction)
                   .getOveridesInvokeMethodCorrectiveAction();
    }

    /**
     * Returns whether <code>result</code> matches the desired result set during construction
     *
     * @param    result the object to compare with the desired result
     *
     * @return whether <code>result</code> matches the desired result set during construction
     */
    private boolean matchesDesiredResult(Object result)
    {
        return ((result == null) && (m_desiredResult == null)) || m_desiredResult.equals(result);
    }


    /** The MBean method name to invoke */
    private String m_methodName;
    /** The parameters to use when invoking */
    private Object[] m_parameters;
    /** The signature of the method to invoke */
    private String[] m_signature;
    /** The result desired from invoking the method on the MBean */
    private Object m_desiredResult;
}
