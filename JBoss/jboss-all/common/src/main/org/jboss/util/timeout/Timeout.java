/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.util.timeout;


/**
 *  The public interface of timeouts.
 *   
 *  @author <a href="osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.1 $
*/
public interface Timeout {
   /**
    *  Cancel this timeout.
    *
    *  It is guaranteed that on return from this method this timer is
    *  no longer active. This means that either it has been cancelled and
    *  the timeout will not happen, or (in case of late cancel) the
    *  timeout has happened and the timeout callback function has returned.
    *
    *  On return from this method this instance should no longer be
    *  used. The reason for this is that an implementation may reuse
    *  cancelled timeouts, and at return the instance may already be
    *  in use for another timeout.
    */
   public void cancel();
}

