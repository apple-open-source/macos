/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * <P>The FinderException exception must be included in the throws clause
 * of every findMETHOD(...) method of an entity Bean's home interface.</P>
 * 
 * <P>The exception is used as a standard application-level exception to,
 * report a failure to find the requested EJB object(s).</P>
 */
public class FinderException extends Exception {

  /**
   * Constructs an FinderException with no detail message.
   */
  public FinderException() {
    super();
  }

  /**
   * Constructs an FinderException with the specified detail message.
   *
   * @param message The detailed message.
   */
  public FinderException(String message) {
    super(message);
  }
}
