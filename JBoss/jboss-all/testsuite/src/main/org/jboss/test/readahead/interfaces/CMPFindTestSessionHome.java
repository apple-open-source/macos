package org.jboss.test.readahead.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;

/**
 * Home interface for finder read-ahead tests
 * 
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 * @version $Id: CMPFindTestSessionHome.java,v 1.1 2001/06/30 04:38:06 danch Exp $
 * 
 * Revision:
 */
public interface CMPFindTestSessionHome extends EJBHome {
   public CMPFindTestSessionRemote create() throws RemoteException, CreateException;
}