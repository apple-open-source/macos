/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.entity.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/**
 * Session facade for entity testing.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.1 $
 */
public interface EntitySession
   extends EJBObject
{
   public void createPathological(String name, boolean pathological)
      throws RemoteException;

   public void removeHomePathological(String name, boolean pathological)
      throws RemoteException;

   public void removePathological(String name, boolean pathological)
      throws RemoteException;

   public void findPathological(String name, boolean pathological)
      throws RemoteException;

   public void getPathological(String name, boolean pathological)
      throws RemoteException;

   public void setPathological(String name, boolean pathological)
      throws RemoteException;
}
