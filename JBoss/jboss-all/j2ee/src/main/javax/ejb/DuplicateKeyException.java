/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * The DuplicateKeyException exception is thrown if an entity EJB
 * object cannot be created because an object with the same key already
 * exists. This exception is thrown by the create methods defined in an
 * enterprise Bean's home interface.
 */
public class DuplicateKeyException extends CreateException {

  /**
   * Constructs an DuplicateKeyException with no detail message. 
   */
  public DuplicateKeyException() {
    super();
  }

  /**
   * Constructs an DuplicateKeyException with the specified detail message.
   *
   * @param message - The detailed message
   */
  public DuplicateKeyException(String message) {
    super(message);
  }
}
