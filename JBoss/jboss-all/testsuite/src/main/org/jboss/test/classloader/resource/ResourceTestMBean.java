package org.jboss.test.classloader.resource;

import org.jboss.system.ServiceMBean;

/** A simple service for testing resource loading
 * @author Adrian.Brock@HappeningTimes.com
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.2 $
 */
public interface ResourceTestMBean extends ServiceMBean
{
   public String getDtdName();
   public void setDtdName(String dtdName);
}
