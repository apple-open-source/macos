/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

import java.rmi.RemoteException;

/**
 * The Handle interface is implemented by all EJB object handles. A handle
 * is an abstraction of a network reference to an EJB object. A handle is
 * intended to be used as a "robust" persistent reference to an EJB object.
 */
public interface Handle extends java.io.Serializable {

  /**
   * Obtains the EJB object reference represented by this handle.
   *
   * @return The EJB object reference represented by this handle.
   * @exception java.rmi.RemoteException - The EJB object could not be obtained because of a
   * system-level failure.
   */
  public EJBObject getEJBObject() throws RemoteException;
}
