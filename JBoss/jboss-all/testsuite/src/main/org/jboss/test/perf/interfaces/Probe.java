package org.jboss.test.perf.interfaces;

/** A trivial stateless session bean for testing round trip call
throughput.
*/
public interface Probe extends javax.ejb.EJBObject
{
   /** Basic test that has no arguments or return values to test the
    bare call invocation overhead without any data serialize.
    */
   public void noop() throws java.rmi.RemoteException;
   /** Basic test that has argument serialization.
    */
   public void ping(String arg) throws java.rmi.RemoteException;
   /** Basic test that has both argument serialization and return
    value serialization.
    */
   public String echo(String arg) throws java.rmi.RemoteException;
}
