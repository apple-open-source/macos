/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.excepiiop.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

public interface ExceptionThrower
   extends EJBObject
{
   public void throwException(int i)
      throws RemoteException, JavaException, IdlException;
}
