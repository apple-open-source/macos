/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * The RemoveException exception is thrown at an attempt to remove an EJB
 * object when the enterprise Bean or the container does not allow the EJB
 * object to be removed.
 */
public class RemoveException extends Exception {

  /**
   * Constructs an RemoveException with no detail message.
   */
  public RemoveException() {
    super();
  }

  /**
   * Constructs an RemoveException with the specified detail message.
   *
   * @param message - The detailed message.
   */
  public RemoveException(String message) {
    super(message);
  }
}
