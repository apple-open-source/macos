/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.tc4;

import java.net.UnknownHostException;

import org.w3c.dom.Element;

import org.jboss.web.AbstractWebContainerMBean;
import org.jboss.web.tomcat.statistics.InvocationStatistics;

/** Management interface for the embedded Catalina service.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.1.1.2.1 $
 */
public interface EmbeddedTomcatServiceMBean extends AbstractWebContainerMBean
{
   /** Get the value to use for the catalina.home System
    property. If not specified this will be determined based on the
    location of the jar containing the org.apache.catalina.startup.Embedded
    class assuming a standard catalina distribution structure.
    */
   public String getCatalinaHome();
   /** Set the value to use for the catalina.home System
     properties. If not specified this will be determined based on the
     location of the jar containing the org.apache.catalina.startup.Embedded
     class assuming a standard catalina distribution structure.
     */
   public void setCatalinaHome(String catalinaHome);

   /** Get the value to use for the catalina.base System
    property. If not specified the catalina.home value will be used
    */
   public String getCatalinaBase();
   /** Set the value to use for the catalina.base */
   public void setCatalinaBase(String catalinaBase);

   /** Access the extended configuration information
    */
   public Element getConfig();
   /** Override the AbstractWebContainerMBean to provide support for extended
    configuration using constructs from the standard server.xml to configure
    additional connectors, etc.
    */
   public void setConfig(Element config);

   /** Get the delete work dirs on undeployment flag.
    @see #setDeleteWorkDirs(boolean)
    */
   public boolean getDeleteWorkDirs();
   /** Set the delete work dirs on undeployment flag. By default catalina
    does not delete its working directories when a context is stopped and
    this can cause jsp pages in redeployments to not be recompiled if the
    timestap of the file in the war has not been updated. This defaults to true.
    */
   public void setDeleteWorkDirs(boolean flag);

   /** Set the snapshot mode in a clustered environment */
   public void setSnapshotMode(String mode);

   /** Get the snapshot mode in a clustered environment */
   public String getSnapshotMode();

   /** Set the snapshot interval in ms for the interval snapshot mode */
   public void setSnapshotInterval(int interval);

   /** Get the snapshot interval */
   public int getSnapshotInterval();

   /** Get the active thread count */
   public int getActiveThreadCount();

   /** Get the maximal active thread count */
   public int getMaxActiveThreadCount();

   /** Get the maximal active thread count */
   public InvocationStatistics getStats();
   /** Reset the stats */
   public void resetStats();

   /** Get the JBoss UCL use flag */
   public boolean getUseJBossWebLoader();
   /** Get the JBoss UCL use flag */
   public void setUseJBossWebLoader(boolean flag);

   /** Get the request attribute name under which the JAAS Subject is store */
   public String getSubjectAttributeName();
   /** Set the request attribute name under which the JAAS Subject will be
    * stored when running with a security mgr that supports JAAS. If this is
    * empty then the Subject will not be store in the request.
    * @param name the HttpServletRequest attribute name to store the Subject 
    */ 
   public void setSubjectAttributeName(String name);
}
