/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

// Standard Java Packages
import java.util.Set;

/**
 * Interface for corrective actions to take on MBeans
 */
public interface CorrectiveAction
    extends Cloneable
{
    /**
     * Returns whether this CorrectiveAction can be applied.
     *
     * @return whether this CorrectiveAction can be applied.
     */
    public boolean canApply();

    /**
     * Apply this CorrectiveAction.
     *
     * @return whether the corrective action succeeded.
     * @throws Exception
     */
    public boolean apply() throws Exception;

    /**
     * Determine if <code>correctiveAction</code> overides this one.
     *
     * @param    correctiveActions the CorrectiveAction to compare with this one.
     *
     * @return whether <code>correctiveAction</code> overides this one.
     */
    public boolean isOverridenBy(final CorrectiveAction correctiveAction);

    /**
     * Determine if any of the CorrectiveActions in <code>correctiveActions</code> overides this one.
     *
     * @param    correctiveActions the Set of CorrectiveActions to compare with this one.
     *
     * @return true if any of the CorrectiveActions in <code>correctiveActions</code> overides this one.
     */
    public boolean isOverridenBy(final Set correctiveActions);

    /**
     * Sets the CorrectiveActionContext of this CorrectiveAction
     *
     * @param    correctiveActionContext the CorrectiveActionContext that this Corrective should use.
     *
     * @return this
     */
    public CorrectiveAction setCorrectiveActionContext(CorrectiveActionContext correctiveActionContext);

    /**
     * Returns the CorrectiveActionContext of this CorrectiveAction
     *
     * @return the CorrectiveActionContext of this CorrectiveAction
     */
    public CorrectiveActionContext getCorrectiveActionContext();

    /**
     * Sets the number of times this CorrectiveAction can be applied
     *
     * @param    numberOfTimesToApply
     *
     * @return this
     */
    public CorrectiveAction setNumberOfTimesToApply(final int numberOfTimesToApply);

    /**
     * Get the total number of times this CorrectiveAction can be applied.
     *
     * @return the total number of times this CorrectiveAction can be applied.
     */
    public int getNumberOfTimesToApply();
}

/*
Example uses of Corrective Actions

<mbean code="org.jbossmx.cluster.watchdog.mbean.Watchdog"
       archive="flibble.jar"
       codebase="flobble/bobble">
    <parameter name="frog" value="lizard"/>
    <method name="addCorrectiveAction">
      // Constructor params
      <param class="org.jbossmx.cluster.watchdog.mbean.watchdog.InvokeMethodCorrectiveAction">
        <param class="java.lang.Object[]">
          <param class="java.lang.String">restartMBean</param>
          <param class="java.lang.Object[]"/>
          <param class="java.lang.String[]"/>
          <param class="java.lang.Object">
            <param class="Boolean">true</param>
          </param>
        </param>
      </param>
      // Number of times to use CorrectiveAction
      <param class="java.lang.Integer">4</param>
    </method>
    <method name="addCorrectiveAction">
      // Constructor Parameters for above CorrectiveAction
      <param class="org.jbossmx.cluster.watchdog.mbean.watchdog.RestartAgentCorrectiveAction"/>
      // Number of times to use CorrectiveAction
      <param class="java.lang.Integer">4</param>
    </method>
    <method name="addCorrectiveAction">
      // Constructor Parameters for above CorrectiveAction
      <param class="org.jbossmx.cluster.watchdog.mbean.watchdog.CallScriptCorrectiveAction">
        <param class="java.lang.Object[]">
          <param class="java.lang.String">/apps/hermes/bin/restartMachine</param>
          <param class="java.lang.Long">30000</param>
        </param>
      </param>
      // Number of times to use CorrectiveAction
      <param class="java.lang.Integer">
        4
      </param>
    </method>
</mbean>

//*/
