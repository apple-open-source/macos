/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.ejb.plugins.lock;


import javax.transaction.Transaction;

import org.jboss.invocation.Invocation;

/**
 * This class has been deprecated.
 * 
 * 
 * Holds all locks for entity beans, not used for stateful. <p>
 *
 * All BeanLocks have a reference count.
 * When the reference count goes to 0, the lock is released from the
 * id -> lock mapping.
 *
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 *
 * @version $Revision: 1.4.4.1 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2001/08/08: billb</b>
 *  <ol>
 *  <li>Initial revision
 *  </ol>
 */
public class MethodOnlyEJBLock extends NoLock
{
   public MethodOnlyEJBLock()
   {
      System.err.println("WARNING: MethodOnlyEJBLock has been deprecated!!!!!");
   }
}

