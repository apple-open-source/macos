/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.ejb;

import java.rmi.RemoteException;

/**
 * The SessionBean interface is implemented by every session enterprise
 * Bean class. The container uses the SessionBean methods to notify the
 * enterprise Bean instances of the instance's life cycle events.
 */
public interface SessionBean extends EnterpriseBean {

  /**
   * <P>The activate method is called when the instance is activated from its "passive" state.
   * The instance should acquire any resource that it has released earlier in the ejbPassivate() method.</P>
   *
   * <P>This method is called with no transaction context.</P>
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
   * <P>The passivate method is called before the instance enters the "passive" state. The instance should
   * release any resources that it can re-acquire later in the ejbActivate() method.</P>
   *
   * <P>After the passivate method completes, the instance must be in a state that allows the container to
   * use the Java Serialization protocol to externalize and store away the instance's state.</P>
   *
   * <P>This method is called with no transaction context.</P>
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
   * <P>A container invokes this method before it ends the life of the session object. This happens
   * as a result of a client's invoking a remove operation, or when a container decides to terminate
   * the session object after a timeout.</P>
   *
   * <P>This method is called with no transaction context.</P>
   *
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   * @exception java.rmi.RemoteException - This exception is defined in the method signature to provide
   * backward compatibility for enterprise beans written for the EJB 1.0 specification. Enterprise beans
   * written for the EJB 1.1 specification should throw the javax.ejb.EJBException instead of this exception.
   * Enterprise beans written for the EJB2.0 and higher specifications must throw the javax.ejb.EJBException
   * instead of this exception.
   */
  public void ejbRemove() throws EJBException, RemoteException;

  /**
   * <P>Set the associated session context. The container calls this method after the instance creation.</P>
   *
   * <P>The enterprise Bean instance should store the reference to the context object in an instance variable.</P>
   *
   * <P>This method is called with no transaction context.</P>
   *
   * @param ctx - A SessionContext interface for the instance.
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   * @exception java.rmi.RemoteException - This exception is defined in the method signature to provide
   * backward compatibility for enterprise beans written for the EJB 1.0 specification. Enterprise beans
   * written for the EJB 1.1 specification should throw the javax.ejb.EJBException instead of this exception.
   * Enterprise beans written for the EJB2.0 and higher specifications must throw the javax.ejb.EJBException
   * instead of this exception.
   */
  public void setSessionContext(SessionContext ctx) throws EJBException, RemoteException;
}
