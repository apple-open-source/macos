/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

/**
 *  This is the callback interface that has to be implemented by objects
 *  interested in receiving notification before and after a transaction
 *  commits or rolls back.
 *
 *  An interested party can give an instance implementing this interface
 *  as an argument to the
 *  {@link Transaction#registerSynchronization(Synchronization) Transaction.registerSynchronization}
 *  method to receive callbacks before and after a transaction commits or
 *  rolls back.
 *
 *  @version $Revision: 1.2 $
 */
public interface Synchronization
{
    /**
     *  This method is invoked before the start of the commit or rollback
     *  process. The method invocation is done in the context of the
     *  transaction that is about to be committed or rolled back.
     */
    public void beforeCompletion();

    /**
     *  This method is invoked after the transaction has committed or
     *  rolled back.
     *
     *  @param status The status of the completed transaction.
     */
    public void afterCompletion(int status);
}
