package org.jboss.test.readahead.interfaces;

import java.util.Collection;
import java.rmi.RemoteException;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

/**
 * Home interface for one of the entities used in read-ahead finder tests.
 * 
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 * @version $Id: CMPFindTestEntityHome.java,v 1.1 2001/06/30 04:38:06 danch Exp $
 * 
 * Revision:
 */
public interface CMPFindTestEntityHome extends EJBHome {
   public CMPFindTestEntityRemote create(String key) throws RemoteException, CreateException;
   public CMPFindTestEntityRemote findByPrimaryKey(String primaryKey) throws RemoteException, FinderException;
   public Collection findAll() throws RemoteException, FinderException;
   public Collection findByCity(String city) throws RemoteException, FinderException;
}