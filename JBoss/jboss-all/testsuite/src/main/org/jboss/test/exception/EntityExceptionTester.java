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
import javax.ejb.EJBException;
import javax.ejb.EJBObject;

/**
 * A test of entity beans exceptions.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public interface EntityExceptionTester
   extends EJBObject
{
    public String getKey() throws RemoteException;

    public void applicationExceptionInTx()
       throws ApplicationException, RemoteException;

    public void applicationExceptionInTxMarkRollback()
       throws ApplicationException, RemoteException;

    public void applicationErrorInTx() throws RemoteException;

    public void ejbExceptionInTx() throws RemoteException;

    public void runtimeExceptionInTx() throws RemoteException;

    public void remoteExceptionInTx() throws RemoteException;

    public void applicationExceptionNewTx()
       throws ApplicationException, RemoteException;

    public void applicationExceptionNewTxMarkRollback()
       throws ApplicationException, RemoteException;

    public void applicationErrorNewTx() throws RemoteException;

    public void ejbExceptionNewTx() throws RemoteException;

    public void runtimeExceptionNewTx() throws RemoteException;

    public void remoteExceptionNewTx()
       throws RemoteException;

    public void applicationExceptionNoTx()
       throws ApplicationException, RemoteException;

    public void applicationErrorNoTx() throws RemoteException;

    public void ejbExceptionNoTx() throws RemoteException;

    public void runtimeExceptionNoTx() throws RemoteException;

    public void remoteExceptionNoTx()
       throws RemoteException;
} 