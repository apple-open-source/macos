package org.jboss.test.classloader.classpath;

import org.jboss.system.ServiceMBean;

/**
 * @author adrian@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public interface ClasspathTestMBean extends ServiceMBean
{
   boolean findResource(String name);
}
