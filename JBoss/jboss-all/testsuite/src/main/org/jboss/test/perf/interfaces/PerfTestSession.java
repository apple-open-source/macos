/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.perf.interfaces;

import java.rmi.RemoteException;

/** A session bean that tests intra-VM EJB call invocation overhead.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.6.2 $
 */
public interface PerfTestSession extends javax.ejb.EJBObject
{
   /** Run the unit tests using Probe and return a report as a string
    */
   public PerfResult runProbeTests(int iterationCount) throws RemoteException;
   /** Run the unit tests using ProbeLocal and return a report as a string
    */
   public PerfResult runProbeLocalTests(int iterationCount) throws RemoteException;
}
