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
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.FinderException;

/**
 * A test of entity beans exceptions.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class EntityExceptionTesterBean
   implements EntityBean
{
    private EntityContext ctx;

    String key;

    public String ejbCreate(String key)
       throws CreateException
    {
       this.key = key;
       return key;
    }

    public void ejbPostCreate(String key)
       throws CreateException
    {
    }

    public String ejbFindByPrimaryKey(String key)
       throws FinderException
    {
       throw new FinderException("Error, bean instance was discarded!");
    }

    public String getKey()
    {
       return key;
    }

    public void applicationExceptionInTx()
       throws ApplicationException
    {
       throw new ApplicationException("Application exception from within " + 
                                      "an inherited transaction");
    }

    public void applicationExceptionInTxMarkRollback()
       throws ApplicationException
    {
       ctx.setRollbackOnly();
       throw new ApplicationException("Application exception from within " + 
                                      "an inherited transaction");
    }

    public void applicationErrorInTx()
    {
       throw new ApplicationError("Application error from within " +
                                  "an inherited transaction");
    }

    public void ejbExceptionInTx()
    {
       throw new EJBException("EJB exception from within " + 
                              "an inherited transaction");
    }

    public void runtimeExceptionInTx()
    {
       throw new RuntimeException("Runtime exception from within " + 
                                  "an inherited transaction");
    }

    public void remoteExceptionInTx()
       throws RemoteException
    {
       throw new RemoteException("Remote exception from within " + 
                                 "an inherited transaction");
    }

    public void applicationExceptionNewTx()
       throws ApplicationException
    {
       throw new ApplicationException("Application exception from within " + 
                                      "a new container transaction");
    }

    public void applicationExceptionNewTxMarkRollback()
       throws ApplicationException
    {
       ctx.setRollbackOnly();
       throw new ApplicationException("Application exception from within " + 
                                      "a new container transaction");
    }

    public void applicationErrorNewTx()
    {
       throw new ApplicationError("Application error from within " +
                                  "a new container transaction");
    }

    public void ejbExceptionNewTx()
    {
       throw new EJBException("EJB exception from within " +
                              "a new container transaction");
    }

    public void runtimeExceptionNewTx()
    {
       throw new RuntimeException("Runtime exception from within " + 
                                  "a new container transaction");
    }

    public void remoteExceptionNewTx()
       throws RemoteException
    {
       throw new RemoteException("Remote exception from within " + 
                                 "a new container transaction");
    }

    public void applicationExceptionNoTx()
       throws ApplicationException
    {
       throw new ApplicationException("Application exception without " +
                                      "a transaction");
    }

    public void applicationErrorNoTx()
    {
       throw new ApplicationError("Application error from within " +
                                  " an inherited transaction");
    }

    public void ejbExceptionNoTx()
    {
       throw new EJBException("EJB exception without " +
                              "a transaction");
    }

    public void runtimeExceptionNoTx()
    {
       throw new RuntimeException("Runtime exception without " +
                                  "a transaction");
    }

    public void remoteExceptionNoTx()
       throws RemoteException
    {
       throw new RemoteException("Remote exception without " +
                                 "a transaction");
    }

    public void setEntityContext(EntityContext ctx)
    {
       this.ctx = ctx;
    }

    public void unsetEntityContext()
    {
    }

    public void ejbLoad()
    {
    }

    public void ejbStore()
    {
    }

    public void ejbActivate()
    {
    }

    public void ejbPassivate()
    {
    }

    public void ejbRemove()
    {
    }
} 