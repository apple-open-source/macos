/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.httpsession.beanimpl.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import java.util.Collection;

import org.jboss.ha.httpsession.interfaces.SerializableHttpSession;

/**
 * Home interface for clustered HTTP session. 
 *
 * @see org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSession
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

public interface ClusteredHTTPSessionHome extends javax.ejb.EJBHome
{
    public static String JNDI_NAME = "clustering/HTTPSession";

    // Constructors
    //
    public ClusteredHTTPSession create (String sessionId) throws RemoteException, CreateException;
    public ClusteredHTTPSession create (String sessionId, SerializableHttpSession session) throws RemoteException, CreateException;

    // Finders
    //
    public ClusteredHTTPSession findByPrimaryKey (String sessionId) throws RemoteException, FinderException;

    // Returns a collection of known HttpSession instances
    //
    public Collection findAll() throws RemoteException, FinderException;

}
