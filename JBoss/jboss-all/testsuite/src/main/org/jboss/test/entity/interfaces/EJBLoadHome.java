/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.entity.interfaces;

import java.rmi.RemoteException;

import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

/**
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.4.1 $
 */
public interface EJBLoadHome
   extends EJBHome
{
   public EJBLoad create(String name)
      throws RemoteException, CreateException;
	
   public EJBLoad findByPrimaryKey(String name)
	throws RemoteException, FinderException;
}
