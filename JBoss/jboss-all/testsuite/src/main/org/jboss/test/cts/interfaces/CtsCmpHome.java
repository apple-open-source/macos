package org.jboss.test.cts.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.DuplicateKeyException;
import javax.ejb.FinderException;

import org.jboss.test.cts.keys.AccountPK;

public interface CtsCmpHome
   extends javax.ejb.EJBHome
{
   public CtsCmp create (AccountPK pk, String personsName)
      throws CreateException, DuplicateKeyException,
             RemoteException;

   public CtsCmp findByPrimaryKey (AccountPK pk)
      throws FinderException, RemoteException;

}
