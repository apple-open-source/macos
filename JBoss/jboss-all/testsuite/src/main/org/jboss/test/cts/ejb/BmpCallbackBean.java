package org.jboss.test.cts.ejb;



import org.jboss.test.cts.keys.*;
import java.rmi.RemoteException;
import java.util.Vector;
import java.util.Collection;
import java.sql.*;
import javax.naming.*;
import javax.ejb.*;
import javax.sql.DataSource;


public class BmpCallbackBean
   implements EntityBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
    EntityContext ctx;

   /**
    * Method ejbCreate
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

   public AccountPK ejbCreate (AccountPK pk, String personsName)
      throws CreateException, DuplicateKeyException, EJBException,
             RemoteException
   {
      log.debug("entry ejbCreate");

      return new AccountPK(pk.getKey());
   }

   /**
    * Method ejbFindByPrimaryKey
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

   public AccountPK ejbFindByPrimaryKey (AccountPK pk)
      throws FinderException, EJBException, RemoteException
   {
      log.debug("entry ejbFindByPrimaryKey");

      return new AccountPK(pk.getKey());
   }

   /**
    * Method ejbPostCreate
    *
    *
    * @param pk
    * @param personsName
    *
    * @throws CreateException
    * @throws DuplicateKeyException
    * @throws EJBException
    * @throws RemoteException
    *
    */

   public void ejbPostCreate (AccountPK pk, String personsName)
      throws CreateException, DuplicateKeyException, EJBException,
             RemoteException
   {
      log.debug("ejbPostCreate (AccountPK, String) called");
   }

   /**
    * Method ejbLoad
    *
    *
    * @throws EJBException
    * @throws RemoteException
    *
    */

   public void ejbLoad ()
      throws EJBException, RemoteException
   {
      log.debug("ejbLoad () called");
   }

   /**
    * Method ejbStore
    *
    *
    * @throws EJBException
    * @throws RemoteException
    *
    */

   public void ejbStore ()
      throws EJBException, RemoteException
   {
      log.debug("ejbStore () called");

   }

   /**
    * Method ejbRemove
    *
    *
    * @throws EJBException
    * @throws RemoteException
    *
    */

   public void ejbRemove ()
      throws EJBException, RemoteException
   {
      log.debug("ejbRemove () called");
   }

   /**
    * Method ejbActivate
    *
    *
    * @throws EJBException
    * @throws RemoteException
    *
    */

   public void ejbActivate ()
      throws EJBException, RemoteException
   {
      log.debug("ejbActivate () called");
   }

   /**
    * Method ejbPassivate
    *
    *
    * @throws EJBException
    * @throws RemoteException
    *
    */

   public void ejbPassivate ()
      throws EJBException, RemoteException
   {
      log.debug("ejbPassivate () called");
   }

   /**
    * Method setEntityContext
    *
    *
    * @param ctx
    *
    * @throws EJBException
    * @throws RemoteException
    *
    */

   public void setEntityContext (EntityContext ctx)
      throws EJBException, RemoteException
   {
      log.debug("setEntityContext (\"" + ctx.getPrimaryKey()
                         + "\") called");

      this.ctx = ctx;

   }

   /**
    * Method unsetEntityContext
    *
    *
    * @throws EJBException
    * @throws RemoteException
    *
    */

   public void unsetEntityContext ()
      throws EJBException, RemoteException
   {
      log.debug("unsetEntityContext () called");

      ctx = null;
   }

}


/*------ Formatted by Jindent 3.23 Basic 1.0 --- http://www.jindent.de ------*/


