/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.entityexc.interfaces;

import java.util.Collection;

import java.rmi.RemoteException;

import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

/**
 *  Home interface of the entity exception test bean.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.1 $
 */
public interface EntityExcHome extends EJBHome
{
  /**
   *  JNDI name of this home. 
   */
  public final String JNDI_NAME = "EntityExc";

  /**
   *  Create a new entity instance.
   */
  public EntityExc create(Integer id, int flags)
    throws MyAppException, CreateException, RemoteException;

  /**
   *  Find by primary key.
   */
  public EntityExc findByPrimaryKey(Integer key, int flags)
    throws MyAppException, FinderException, RemoteException;
 
  /**
   *  Find all beans in this interface.
   */
  public Collection findAll(int flags)
    throws MyAppException, FinderException, RemoteException;
 
  /**
   *  Reset the database to a known state.
   *  This is used for initializing, and should be called after deployment
   *  but before the tests start.
   *  It will create the database table, if it does not exist, or is not
   *  correctly defined.
   *  If the database table is not empty, all records in it will be deleted.
   */
  public void resetDatabase()
    throws RemoteException;
}
