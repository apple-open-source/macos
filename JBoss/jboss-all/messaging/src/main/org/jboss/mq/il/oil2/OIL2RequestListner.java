/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil2;

/**
 * 
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision: 1.2 $
 */
public interface OIL2RequestListner
{
   void handleRequest(OIL2Request request);
   void handleConnectionException(Exception e);   
}
