/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.httpsession.beanimpl.interfaces;

/**
 * Remote interface for clustered HTTP sessions.
 *
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSessionBusiness
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>31. décembre 2001 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li> 
 * </ul>
 */

public interface ClusteredHTTPSession extends ClusteredHTTPSessionBusiness, javax.ejb.EJBObject
{

}
