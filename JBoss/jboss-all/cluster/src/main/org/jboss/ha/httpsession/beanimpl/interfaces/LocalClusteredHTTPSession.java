/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.httpsession.beanimpl.interfaces;

/**
 * Local interface for clustered HTTP sessions.
 *
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSessionBusiness
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>20020105 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li> 
 * </ul>
 */

public interface LocalClusteredHTTPSession extends ClusteredHTTPSessionBusiness, javax.ejb.EJBLocalObject
{

}
