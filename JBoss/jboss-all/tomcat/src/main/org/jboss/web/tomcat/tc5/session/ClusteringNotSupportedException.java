/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.tc5.session;


/** This exception is thrown when the clustered HTTPSession-service
 *  is not found

 @see org.jboss.ha.httpsession.server.ClusteredHTTPSessionService

 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.2.1 $
 */
public class ClusteringNotSupportedException extends Exception
{
   public ClusteringNotSupportedException(String message)
   {
      super(message);
   }
}
