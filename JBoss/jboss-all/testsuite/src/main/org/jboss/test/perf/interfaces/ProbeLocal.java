package org.jboss.test.perf.interfaces;

import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;

/** A trivial stateless session bean for testing round trip call
throughput.
@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
*/
public interface ProbeLocal extends EJBLocalObject
{
   /** Basic test that has no arguments or return values to test the
    bare call invocation overhead without any data serialize.
    */
   public void noop() throws EJBException;
   /** Basic test that has argument serialization.
    */
   public void ping(String arg) throws EJBException;
   /** Basic test that has both argument serialization and return
    value serialization.
    */
   public String echo(String arg) throws EJBException;
}
