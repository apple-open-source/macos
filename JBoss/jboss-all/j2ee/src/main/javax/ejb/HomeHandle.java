/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

import java.rmi.RemoteException;

/**
 * The HomeHandle interface is implemented by all home object handles. A
 * handle is an abstraction of a network reference to a home object. A
 * handle is intended to be used as a "robust" persistent reference to a
 * home object.
 */
public interface HomeHandle extends java.io.Serializable {

  /**
   * Obtains the home object represented by this handle.
   *
   * @return The home object represented by this handle.
   * @exception java.rmi.RemoteException - The home object could not be obtained because of a
   * system-level failure.
   */
  public EJBHome getEJBHome() throws RemoteException;
}
