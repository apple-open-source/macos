/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * This exception indicates that a request carried a null transaction context,
 * but the target object requires an activate transaction.
 */
public class TransactionRequiredLocalException extends EJBException {

  /**
   * Constructs a TransactionRequiredLocalException with no detail message.
   */
  public TransactionRequiredLocalException() {
    super();
  }

  /**
   * Constructs a TransactionRequiredLocalException with the specified detailed message.
   *
   * @param message - The detailed message.
   */
  public TransactionRequiredLocalException(String message) {
    super(message);
  }
}
