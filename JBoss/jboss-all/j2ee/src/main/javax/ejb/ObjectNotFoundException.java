/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * <p>The ObjectNotFoundException exception is thrown by a finder method
 * to indicate that the specified EJB object does not exist.</p>
 *
 * <p>Only the finder methods that are declared to return a single EJB object
 * use this exception. This exception should not be thrown by finder methods
 * that return a collection of EJB objects (they should return an empty collection
 * instead).</p>
 */
public class ObjectNotFoundException extends FinderException {

  /**
   * Constructs an ObjectNotFoundException with no detail message.
   */
  public ObjectNotFoundException() {
    super();
  }

  /**
   * Constructs an ObjectNotFoundException with the specified detail message.
   *
   * @param message - The detailed message.
   */
  public ObjectNotFoundException(String message) {
    super(message);
  }
}
