/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.perf.test;

/** An MBean that tests intra-VM EJB call invocation overhead.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1 $
 */
public interface PerfTestMBean
{
   /** Run the unit tests and return a report as a string
    */
   public String runTests(int iterationCount);
}
