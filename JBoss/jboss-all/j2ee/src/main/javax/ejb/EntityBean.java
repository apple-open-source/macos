/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

import java.rmi.RemoteException;

/**
 * The EntityBean interface is implemented by every entity enterprise
 * Bean class. The container uses the EntityBean methods to notify the
 * enterprise Bean instances of the instance's life cycle events.
 */
public interface EntityBean extends EnterpriseBean {

  /**
   * <P>A container invokes this method when the instance is taken out of the pool of available
   * instances to become associated with a specific EJB object. This method transitions the instance
   * to the ready state.</P>
   *
   * <P>This method executes in an unspecified transaction context.</P>
   *
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   * @exception java.rmi.RemoteException - This exception is defined in the method signature to provide
   * backward compatibility for enterprise beans written for the EJB 1.0 specification. Enterprise beans
   * written for the EJB 1.1 specification should throw the javax.ejb.EJBException instead of this exception.
   * Enterprise beans written for the EJB2.0 and higher specifications must throw the javax.ejb.EJBException
   * instead of this exception.
   */
  public void ejbActivate() throws EJBException, RemoteException;

  /**
   * <P>A container invokes this method to instruct the instance to synchronize its state by loading
   * it state from the underlying database.</P>
   *
   * <P>This method always executes in the transaction context determined by the value of the transaction
   * attribute in the deployment descriptor.</P>
   *
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   * @exception java.rmi.RemoteException - This exception is defined in the method signature to provide
   * backward compatibility for enterprise beans written for the EJB 1.0 specification. Enterprise beans
   * written for the EJB 1.1 specification should throw the javax.ejb.EJBException instead of this exception.
   * Enterprise beans written for the EJB2.0 and higher specifications must throw the javax.ejb.EJBException
   * instead of this exception.
   */
  public void ejbLoad() throws EJBException, RemoteException;

  /**
   * <P>A container invokes this method on an instance before the instance becomes disassociated with
   * a specific EJB object. After this method completes, the container will place the instance into the
   * pool of available instances.</P>
   *
   * <P>This method executes in an unspecified transaction context.</P>
   *
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   * @exception java.rmi.RemoteException - This exception is defined in the method signature to provide
   * backward compatibility for enterprise beans written for the EJB 1.0 specification. Enterprise beans
   * written for the EJB 1.1 specification should throw the javax.ejb.EJBException instead of this exception.
   * Enterprise beans written for the EJB2.0 and higher specifications must throw the javax.ejb.EJBException
   * instead of this exception.
   */
  public void ejbPassivate() throws EJBException, RemoteException;

  /**
   * <P>A container invokes this method before it removes the EJB object that is currently associated
   * with the instance. This method is invoked when a client invokes a remove operation on the enterprise
   * Bean's home interface or the EJB object's remote interface. This method transitions the instance from
   * the ready state to the pool of available instances.</P>
   *
   * <P>This method is called in the transaction context of the remove operation.</P>
   *
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   * @exception java.rmi.RemoteException - This exception is defined in the method signature to provide
   * backward compatibility for enterprise beans written for the EJB 1.0 specification. Enterprise beans
   * written for the EJB 1.1 specification should throw the javax.ejb.EJBException instead of this exception.
   * Enterprise beans written for the EJB2.0 and higher specifications must throw the javax.ejb.EJBException
   * instead of this exception.
   */
  public void ejbRemove() throws RemoveException, EJBException, RemoteException;

  /**
   * <P>A container invokes this method to instruct the instance to synchronize its state by storing it to
   * the underlying database.</P>
   *
   * <P>This method always executes in the transaction context determined by the value of the transaction
   * attribute in the deployment descriptor.</P>
   *
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   * @exception java.rmi.RemoteException - This exception is defined in the method signature to provide
   * backward compatibility for enterprise beans written for the EJB 1.0 specification. Enterprise beans
   * written for the EJB 1.1 specification should throw the javax.ejb.EJBException instead of this exception.
   * Enterprise beans written for the EJB2.0 and higher specifications must throw the javax.ejb.EJBException
   * instead of this exception.
   */
  public void ejbStore() throws EJBException, RemoteException;

  /**
   * <P>Set the associated entity context. The container invokes this method on an instance
   * after the instance has been created.</P>
   *
   * <P>This method is called in an unspecified transaction context.</P>
   *
   * @param ctx - An EntityContext interface for the instance. The instance should store the
   * reference to the context in an instance variable.
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   * @exception java.rmi.RemoteException - This exception is defined in the method signature to provide
   * backward compatibility for enterprise beans written for the EJB 1.0 specification. Enterprise beans
   * written for the EJB 1.1 specification should throw the javax.ejb.EJBException instead of this exception.
   * Enterprise beans written for the EJB2.0 and higher specifications must throw the javax.ejb.EJBException
   * instead of this exception.
   */
  public void setEntityContext(EntityContext ctx) throws EJBException, RemoteException;

  /**
   * <P>Unset the associated entity context. The container calls this method before removing the instance.</P>
   *
   * <P>This is the last method that the container invokes on the instance. The Java garbage collector
   * will eventually invoke the finalize() method on the instance.</P>
   *
   * <P>This method is called in an unspecified transaction context.</P>
   *
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   * @exception java.rmi.RemoteException - This exception is defined in the method signature to provide
   * backward compatibility for enterprise beans written for the EJB 1.0 specification. Enterprise beans
   * written for the EJB 1.1 specification should throw the javax.ejb.EJBException instead of this exception.
   * Enterprise beans written for the EJB2.0 and higher specifications must throw the javax.ejb.EJBException
   * instead of this exception.
   */
  public void unsetEntityContext() throws EJBException, RemoteException;
}
