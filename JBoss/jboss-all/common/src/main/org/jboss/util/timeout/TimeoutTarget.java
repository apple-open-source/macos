/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.util.timeout;


/**
 *  The interface of objects that can receive timeouts.
 *   
 *  @author <a href="osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.1 $
*/
public interface TimeoutTarget {
   /**
    *  The timeout callback function is invoked when the timeout expires.
    */
   public void timedOut(Timeout timeout);
}

