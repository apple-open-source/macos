/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.entity.interfaces;

import java.rmi.RemoteException;

import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**
 * Session facade for entity testing.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.1 $
 */
public interface EntitySessionHome
   extends EJBHome
{
   public EntitySession create()
      throws RemoteException, CreateException;
}
