/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * A NoSuchObjectLocalException is thrown if an attempt is made to invoke
 * a method on an object that no longer exists.
 */
public class NoSuchObjectLocalException extends EJBException {

  /**
   * Constructs a NoSuchObjectLocalException with no detail message.
   */
  public NoSuchObjectLocalException() {
    super();
  }

  /**
   * Constructs a NoSuchObjectLocalException with the specified detailed message.
   *
   * @param message - The detailed message.
   */
  public NoSuchObjectLocalException(String message) {
    super(message);
  }

  /**
   * Constructs a NoSuchObjectLocalException with the specified detail message and a
   * nested exception.
   *
   * @param message - The detailed message.
   * @param ex - The originally thrown exception.
   */
  public NoSuchObjectLocalException(String message, Exception ex) {
    super(message,ex);
  }
}
