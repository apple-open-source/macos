/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.entity.interfaces;

import java.rmi.RemoteException;

import javax.ejb.EJBObject;

/**
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.4.1 $
 */
public interface EJBLoad	
   extends EJBObject
{
   public String getName() throws RemoteException;
   public boolean wasEJBLoadCalled() throws RemoteException;
   public void noTransaction() throws RemoteException;
}

