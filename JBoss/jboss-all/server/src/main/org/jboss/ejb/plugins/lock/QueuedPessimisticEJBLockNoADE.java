package org.jboss.ejb.plugins.lock;

/** A subclass of QueuedPessimisticEJBLock that disables the deadlock
 * detection of QueuedPessimisticEJBLock.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class QueuedPessimisticEJBLockNoADE extends QueuedPessimisticEJBLock
{
   public QueuedPessimisticEJBLockNoADE()
   {
      setDeadlockDetection(false);
   }
}
