/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

/**
 * <p>The EJBLocalHome interface must be extended by all enterprise Beans'
 * local home interfaces. An enterprise Bean's local home interface defines
 * the methods that allow local clients to create, find, and remove EJB
 * objects, as well as home business methods that are not specific to a
 * bean instance (session Beans do not have finders and home business methods).</p>
 *
 * <p>The local home interface is defined by the enterprise Bean provider
 * and implemented by the enterprise Bean container.</p>
 */
public interface EJBLocalHome {

  /**
   * <p>Remove an EJB object identified by its primary key.</p>
   *
   * <p>This method can only be used by local clients of an entity bean.
   * An attempt to call this method on a session bean will result in an EJBException.</p>
   *
   * @param primaryKey - The primary key
   * @exception RemoveException - Thrown if the enterprise Bean or the container does
   *                              not allow the client to remove the object.
   * @exception EJBException - Thrown when the method failed due to a system-level failure.
   */
  public void remove(java.lang.Object primaryKey)
    throws RemoveException, EJBException;
}
