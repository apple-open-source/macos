/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * <p>The EJBLocalObject interface must be extended by all enterprise
 * Beans' local interfaces. An enterprise Bean's local interface provides
 * the local client view of an EJB object. An enterprise Bean's local interface
 * defines the business methods callable by local clients.</p>
 *
 * <p>The enterprise Bean's local interface is defined by the enterprise Bean
 * provider and implemented by the enterprise Bean container.</p>
 */
public interface EJBLocalObject {

  /**
   * Obtain the enterprise Bean's local home interface. The local home interface
   * defines the enterprise Bean's create, finder, remove, and home business methods
   * that are available to local clients.
   *
   * @return A reference to the enterprise Bean's local home interface.
   * @exception EJBException - Thrown when the method failed due to a system-level failure.
   */
  public EJBLocalHome getEJBLocalHome()
    throws EJBException;

  /**
   * <p>Obtain the primary key of the EJB local object.</p>
   *
   * <p>This method can be called on an entity bean. An attempt to invoke this method
   * on a session Bean will result in an EJBException.</p>
   *
   * @return The EJB local object's primary key.
   * @exception EJBException - Thrown when the method failed due to a system-level failure.
   */
  public java.lang.Object getPrimaryKey()
    throws EJBException;

  /**
   * Remove the EJB local object.
   *
   * @exception RemoveException - The enterprise Bean or the container does not allow
   *                              destruction of the object.
   * @exception EJBException - Thrown when the method failed due to a system-level failure.
   */
  public void remove()
    throws RemoveException, EJBException;

  /**
   * Test if a given EJB local object is identical to the invoked EJB local object.
   *
   * @param obj - An object to test for identity with the invoked object.
   * @return True if the given EJB local object is identical to the invoked object, false otherwise.
   * @exception EJBException - Thrown when the method failed due to a system-level failure.
   */
  public boolean isIdentical(EJBLocalObject obj)
    throws EJBException;
}
