package org.jboss.test.cts.interfaces;



import org.jboss.test.cts.keys.*;
import java.rmi.RemoteException;
import java.util.Collection;
import javax.ejb.*;


/**
 * Interface CtsBmpHome
 *
 *
 * @author
 * @version %I%, %G%
 */

public interface CtsBmpHome
   extends EJBHome
{

   /**
    * Method create
    *
    *
    * @param pk
    * @param personsName
    *
    * @return
    *
    * @throws CreateException
    * @throws DuplicateKeyException
    * @throws EJBException
    * @throws RemoteException
    *
    */

   public CtsBmp create (AccountPK pk, String personsName)
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

   public CtsBmp findByPrimaryKey (AccountPK pk)
      throws FinderException, EJBException, RemoteException;

   /**
    * Method findAll
    *
    *
    * @return
    *
    * @throws EJBException
    * @throws FinderException
    * @throws RemoteException
    *
    */

   public Collection findAll ()
      throws EJBException, FinderException, RemoteException;

   /**
    * Method findByPersonsName
    *
    *
    * @return
    *
    * @throws EJBException
    * @throws FinderException
    * @throws RemoteException
    *
    */
   public Collection findByPersonsName (String guysName )
      throws EJBException, FinderException, RemoteException;


}


/*------ Formatted by Jindent 3.23 Basic 1.0 --- http://www.jindent.de ------*/




