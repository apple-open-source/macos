package org.jboss.test.perf.ejb;

import javax.ejb.CreateException;

import org.jboss.test.util.ejb.SessionSupport;

/** The Probe and ProbeHome interface implementations. Don't put any
 logging in the methods as this class is used to test the raw call invocation
 overhead.
*/
public class ProbeBean extends SessionSupport
{

// Begin ProbeHome interface methods
  public void ejbCreate() throws CreateException
  {
  }
// End ProbeHome interface methods

// Begin Probe interface methods
   /** Basic test that has no arguments or return values to test the
    bare call invocation overhead without any data serialize.
    */
   public void noop()
   {
   }
   /** Basic test that has argument serialization.
    */
   public void ping(String arg)
   {
   }
   /** Basic test that has both argument serialization and return
    value serialization.
    */
   public String echo(String arg)
   {
      return arg;
   }
// End Probe interface methods
}
