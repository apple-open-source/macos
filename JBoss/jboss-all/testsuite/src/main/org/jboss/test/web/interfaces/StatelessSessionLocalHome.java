package org.jboss.test.web.interfaces;

import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;

/** A trivial local SessionBean home interface.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public interface StatelessSessionLocalHome extends EJBLocalHome
{
    public StatelessSessionLocal create() throws CreateException;
}
