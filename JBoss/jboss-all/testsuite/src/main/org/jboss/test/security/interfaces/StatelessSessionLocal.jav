package org.jboss.test.security.interfaces;

import javax.ejb.EJBException;
import javax.ejb.EJBLocalObject;

/** A simple local statless session interface.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public interface StatelessSessionLocal extends EJBLocalObject
{
   /** Basic test that has no arguments or return values to test the
    bare call invocation overhead without any data serialize.
    */
   public void noop();
   /** Basic test that has both argument serialization and return
    value serialization.
    */
   public String echo(String arg);
}
