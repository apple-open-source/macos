/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.ejb;

import java.rmi.RemoteException;

/**
 * The MessageDrivenBean interface is implemented by every message driven
 * enterprise bean class.  The container uses the MessageDrivenBean methods
 * to notify the enterprise bean instances of the instance's life cycle events.
 */
public interface MessageDrivenBean extends EnterpriseBean {

  /**
   * <P>A container invokes this method before it ends the life of the message-driven object.
   * This happens when a container decides to terminate the message-driven object.</P>
   *
   * <P>This method is called with no transaction context.</P>
   *
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   */
  public void ejbRemove() throws EJBException;

  /**
   * <P>Set the associated message-driven context. The container calls this method after the
   * instance creation.</P>
   *
   * <P>The enterprise Bean instance should store the reference to the context object in an instance
   * variable.</P>
   *
   * <P>This method is called with no transaction context.</P>
   *
   * @param ctx - A MessageDrivenContext interface for the instance.
   * @exception EJBException - Thrown by the method to indicate a failure caused by a system-level error.
   */
  public void setMessageDrivenContext( MessageDrivenContext ctx ) throws EJBException;
}
