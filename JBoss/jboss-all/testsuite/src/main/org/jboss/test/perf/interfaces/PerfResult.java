/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.perf.interfaces;

import java.io.Serializable;

/** The data object returned by PerfTestSession
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.1 $
 */
public class PerfResult implements Serializable
{
   public Exception error;
   public String report;
}
