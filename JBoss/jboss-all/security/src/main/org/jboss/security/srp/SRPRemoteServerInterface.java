/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp;

import java.rmi.Remote;

/** An RMI version of the SRPServerInterface. This interface simply extends both
the SRPServerInterface interface and the java.rmi.Remote to create an
RMI legal interface.

@author Scott.Stark@jboss.org
@version $Revision: 1.3.4.1 $
*/
public interface SRPRemoteServerInterface extends Remote, SRPServerInterface
{
    // All methods come from the SRPServerInterface
}
