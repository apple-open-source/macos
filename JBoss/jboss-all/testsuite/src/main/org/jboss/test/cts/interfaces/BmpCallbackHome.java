package org.jboss.test.cts.interfaces;

import org.jboss.test.cts.keys.*;
import java.rmi.RemoteException;
import java.util.Collection;
import javax.ejb.*;


public interface BmpCallbackHome
   extends EJBHome
{
   public BmpCallback create (AccountPK pk, String personsName)
      throws CreateException, DuplicateKeyException, EJBException,
             RemoteException;

   /**
    * Method findByPrimaryKey
    *
    *
    * @param pk
    *
    * @return
    *
    * @throws EJBException
    * @throws FinderException
    * @throws RemoteException
    *
    */

   public BmpCallback findByPrimaryKey (AccountPK pk)
      throws FinderException, EJBException, RemoteException;

}


/*------ Formatted by Jindent 3.23 Basic 1.0 --- http://www.jindent.de ------*/




