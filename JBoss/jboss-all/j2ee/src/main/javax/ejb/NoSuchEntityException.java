/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * <p>The NoSuchEntityException exception is thrown by an Entity Bean
 * instance to its container to report that the invoked business method
 * or callback method could not be completed because of the underlying
 * entity was removed from the database.</p>
 *
 * <p>This exception may be thrown by the bean class methods that implement
 * the business methods defined in the bean's component interface; and by the
 * ejbLoad and ejbStore methods.</p>
 */
public class NoSuchEntityException extends EJBException {

  /**
   * Constructs a NoSuchEntityException with no detail message.
   */
  public NoSuchEntityException() {
    super();
  }

  /**
   * Constructs a NoSuchEntityException that embeds the originally thrown exception.
   *
   * @param ex - The originally thrown exception.
   */
  public NoSuchEntityException(Exception ex) {
    super(ex);
  }

  /**
   * Constructs a NoSuchEntityException with the specified detailed message.
   *
   * @param message - The detailed message.
   */
  public NoSuchEntityException(String message) {
    super(message);
  }
}
