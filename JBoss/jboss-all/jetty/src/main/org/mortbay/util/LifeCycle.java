// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: LifeCycle.java,v 1.15.2.4 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;/* ------------------------------------------------------------ */
/** A component LifeCycle.
 * Represents the life cycle interface for an abstract
 * software component. 
 *
 * @version $Id: LifeCycle.java,v 1.15.2.4 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public interface LifeCycle
{
    /* ------------------------------------------------------------ */
    /** Start the LifeCycle.
     * @exception Exception An arbitrary exception may be thrown.
     */
    public void start()
        throws Exception;
    
    /* ------------------------------------------------------------ */
    /** Stop the LifeCycle.
     * The LifeCycle may wait for current activities to complete
     * normally, but it can be interrupted.
     * @exception InterruptedException Stopping a lifecycle is rarely atomic
     * and may be interrupted by another thread.  If this happens
     * InterruptedException is throw and the component will be in an
     * indeterminant state and should probably be discarded.
     */
    public void stop()
        throws InterruptedException;
   
    /* ------------------------------------------------------------ */
    /** 
     * @return True if the LifeCycle has been started. 
     */
    public boolean isStarted();
}

