/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.classloader.concurrentload;

import org.jboss.mx.util.ObjectNameFactory;
import javax.management.ObjectName;

import org.jboss.system.Service;
import org.jboss.system.ServiceMBean;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>16. mai 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface ConcurrentLoaderMBean
   extends Service, ServiceMBean
{
   /** The default object name. */
   ObjectName OBJECT_NAME = ObjectNameFactory.create("test:service=ConcurrentLoader");
   
}
