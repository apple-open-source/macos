/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;

import org.jboss.ejb.StatefulSessionEnterpriseContext;

/**
 *  Tag interface to differentiate standard PersistentManager with HA one.
 *
 *  @see StatefulHASessionPersistenceManager
 *  @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 *  @version $Revision: 1.1.4.1 $
 *
 *   <p><b>Revisions:</b>
 */

public interface HAPersistentManager
{
   public void synchroSession (StatefulSessionEnterpriseContext ctx) throws java.rmi.RemoteException;
}

