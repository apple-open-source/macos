/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * This exception indicates that the transaction associated with processing
 * of the request has been rolled back, or marked to roll back. Thus the
 * requested operation either could not be performed or was not performed
 * because further computation on behalf of the transaction would be fruitless.
 */
public class TransactionRolledbackLocalException extends EJBException {

  /**
   * Constructs a TransactionRolledbackLocalException with no detail message.
   */
  public TransactionRolledbackLocalException() {
    super();
  }

  /**
   * Constructs a TransactionRolledbackLocalException with the specified detailed message.
   *
   * @param message - The detailed message.
   */
  public TransactionRolledbackLocalException(String message) {
    super(message);
  }

  /**
   * Constructs a TransactionRolledbackLocalException with the specified detail
   * message and a nested exception.
   *
   * @param message - The detailed message.
   * @param ex - The originally thrown exception.
   */
  public TransactionRolledbackLocalException(String message,Exception ex) {
    super(message,ex);
  }
}
