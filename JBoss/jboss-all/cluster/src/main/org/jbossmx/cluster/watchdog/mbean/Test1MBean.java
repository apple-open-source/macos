/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

/**
 * Interface for a Test MBean
 *
 * @author Stacy Curl
 */
public interface Test1MBean
    extends StartableMBean
{
    /**
     * Sets whether calls to StartableMBean methods will succeed
     *
     * @param    success whether calls to StartableMBean methods will succeed
     */
    public void setCallSuccess(boolean success);

    /**
     * Gets whether calls to StartableMBean methods will succeed
     *
     * @return whether calls to StartableMBean methods will succeed
     */
    public boolean getCallSuccess();

    /**
     * Sets how long calls to StartableMBean methods will take
     *
     * @param    milliseconds how long calls to StartableMBean methods will take
     */
    public void setCallDelay(long milliseconds);

    /**
     * Gets how long calls to StartableMBean methods will take
     *
     * @return how long calls to StartableMBean methods will take
     */
    public long getCallDelay();
}
