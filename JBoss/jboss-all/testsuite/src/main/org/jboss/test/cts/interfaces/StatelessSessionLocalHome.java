package org.jboss.test.cts.interfaces;

import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;

/** Basic stateless session local home
 *
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.1.2.1 $
 */
public interface StatelessSessionLocalHome
   extends EJBLocalHome
{
   public StatelessSessionLocal create ()
      throws CreateException;
}
