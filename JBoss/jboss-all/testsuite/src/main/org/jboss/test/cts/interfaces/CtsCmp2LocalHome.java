package org.jboss.test.cts.interfaces;

import org.jboss.test.cts.keys.AccountPK;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

/**
 @author Scott.Stark@jboss.org
 @version $Revision: 1.2 $
 */
public interface CtsCmp2LocalHome extends javax.ejb.EJBLocalHome
{
   public CtsCmp2Local create(String key, String data)
      throws CreateException;

   public CtsCmp2Local findByPrimaryKey(String key)
      throws FinderException;

}
