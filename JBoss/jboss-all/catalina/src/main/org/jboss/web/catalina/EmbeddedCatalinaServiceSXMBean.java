/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.catalina;

import java.net.UnknownHostException;

import org.w3c.dom.Element;

import org.jboss.web.AbstractWebContainerMBean;

/** Management interface for the embedded Catalina service.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.8.2.2 $
 */
public interface EmbeddedCatalinaServiceSXMBean extends AbstractWebContainerMBean
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

   /** Flag for the standard Java2 parent delegation class loading model
    rather than the servlet 2.3 load from war first model
    */
   public boolean getJava2ClassLoadingCompliance();
   /** Enable the standard Java2 parent delegation class loading model
    rather than the servlet 2.3 load from war first model
    */
   public void setJava2ClassLoadingCompliance(boolean compliance);

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
	       
   /** Set the snapshot-manager economic mode. That means that it only replicates a session if it is modified */
   public void setEconomicSnapshotting(boolean flag);

   /** Get the snapshot-manager economic mode */
   public boolean getEconomicSnapshotting();
}
