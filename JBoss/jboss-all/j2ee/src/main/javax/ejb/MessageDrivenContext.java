/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.ejb;

/**
 * <P>The MessageDrivenContext interface provides an instance with access
 * to the container-provided runtime context of a message-driven enterprise
 * bean instance. The container passes the MessageDrivenContext interface
 * to an entity enterprise Bean instance after the instance has been
 * created.</P>
 * 
 * <P>The MessageDrivenContext interface remains associated with the
 * instance for the lifetime of the instance.
 */

public interface MessageDrivenContext extends EJBContext {
}
