/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.varia.autonumber;

import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import java.rmi.RemoteException;

/**
 * ???
 * 
 * @author <a href="mailto:michel.anke@wolmail.nl">Michel de Groot</a>
 * @version $Revision: 1.1 $
 */
public interface AutoNumberHome
   extends EJBHome
{
   /**
    * Creates an AutoNumber of given name.
    * 
    * @param name the name of the AutoNumber
    */
   AutoNumber create(String name) throws CreateException, RemoteException;
   
   /**
    * Finds an AutoNumber by its name.
    */
   AutoNumber findByPrimaryKey(String name) throws FinderException, RemoteException;
}
