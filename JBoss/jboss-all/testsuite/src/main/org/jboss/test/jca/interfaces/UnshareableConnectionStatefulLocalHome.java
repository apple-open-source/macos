package org.jboss.test.jca.interfaces;

import javax.ejb.CreateException;
import javax.ejb.EJBLocalHome;

public interface UnshareableConnectionStatefulLocalHome 
   extends EJBLocalHome
{
	public UnshareableConnectionStatefulLocal create()
		throws CreateException;
}
