/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.exception;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;
import javax.ejb.FinderException;

/**
 * A test of entity beans exceptions.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public interface EntityExceptionTesterHome
   extends EJBHome
{
    public EntityExceptionTester create(String key)
       throws CreateException, RemoteException;

    public EntityExceptionTester findByPrimaryKey(String key)
       throws FinderException, RemoteException;
} 