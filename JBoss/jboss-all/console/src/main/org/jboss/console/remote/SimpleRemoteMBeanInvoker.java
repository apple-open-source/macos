/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.remote;

import javax.management.ObjectName;

/**
 * Basic adaptor interface for remote MBeanServer. Don't use JMX class to avoid
 * having all the JMX stuff loaded in the applet. This part may be implemented
 * differently later (more clean integration with remote features)
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>21. avril 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface SimpleRemoteMBeanInvoker
{
   public Object invoke(ObjectName name, String operationName, Object[] params, String[] signature)
            throws Exception;

   public Object getAttribute(ObjectName name, String attr)
            throws Exception;

}
