/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.service;

import java.io.IOException;

import org.jboss.system.ServiceMBean;

/** An mbean interface for testing https URLs inside the JBoss server.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.1 $
 */
public interface HttpsClientMBean extends ServiceMBean
{
   /** Read the contents of the given URL and return it. */
   public String readURL(String urlString) throws IOException;
}
