package org.jboss.test.cts.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/** A stateless session interface

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.2.1 $
 */
public interface StrictlyPooledSession extends EJBObject
{
  public void methodA() throws RemoteException;
}