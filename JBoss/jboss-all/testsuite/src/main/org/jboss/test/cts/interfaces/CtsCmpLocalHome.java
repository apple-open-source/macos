package org.jboss.test.cts.interfaces;

import org.jboss.test.cts.keys.AccountPK;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

public interface CtsCmpLocalHome extends javax.ejb.EJBLocalHome
{
   public CtsCmpLocal create(AccountPK pk, String personsName)
      throws CreateException;

   public CtsCmpLocal findByPrimaryKey(AccountPK pk)
      throws FinderException;

}
