/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.cmp2.lob;

import java.rmi.RemoteException;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import java.util.Collection;

/**
 * Remote home interface for a LOBBean.
 *
 * @see javax.ejb.EJBHome.
 *
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 * @author  <a href="mailto:steve@resolvesw.com">Steve Coy</a>
 *
 */
public interface LOBHome extends EJBHome
{
   public LOB create(Integer id)
      throws CreateException, RemoteException;

   public LOB findByPrimaryKey(Integer id)
      throws FinderException, RemoteException;

   public Collection findAll()
      throws FinderException, RemoteException;

}
