package org.jboss.test.perf.interfaces;

import javax.ejb.EJBException;
import javax.ejb.EJBLocalHome;
import javax.ejb.CreateException;

/** A trivial stateless session bean for testing round trip call
throughput.
@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
*/
public interface ProbeLocalHome extends EJBLocalHome
{
   ProbeLocal create() throws CreateException, EJBException;
}
