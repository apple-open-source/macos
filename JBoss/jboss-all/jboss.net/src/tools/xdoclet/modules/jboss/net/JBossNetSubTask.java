/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: JBossNetSubTask.java,v 1.1.2.2 2003/04/09 06:59:22 cgjung Exp $

package xdoclet.modules.jboss.net;

import xdoclet.XmlSubTask;

/**
 * JBossNetSubTask is an xml-based ant task that comes with a few
 * useful tags to support the generation of a suitable web-service.xml 
 * from EJB sources and the JBossNet template.
 * @ant.element display-name="JBoss.Net" name="jbossnet" parent="xdoclet.modules.ejb.EjbDocletTask"
 * @author Frederick M. Brier
 * @author jung
 * @author Jason Essington
 * @since May 28, 2002
 * @version $Revision: 1.1.2.2 $
 */

public abstract class JBossNetSubTask extends XmlSubTask {
   public final static String SUBTASK_NAME= "jbossnet";

   private final static String SOAP_SCHEMA= "http://xml.apache.org/axis/wsdd/";
   private final static String JBOSSNET_DTD_SYSTEMID_31=
      "http://www.jboss.org/j2ee/dtd/jbossnet_3_1.dtd";
   private static String GENERATED_FILE_NAME= "web-service.xml";
   protected String applicationName= "jboss-net-web-application";
   protected String targetNameSpace= "http://www.jboss.org/net";
   protected String prefix= "jboss-net";

   public JBossNetSubTask() {
      setTemplateURL(
         getClass().getClassLoader().getResource(getTemplateName()));
      setDestinationFile(GENERATED_FILE_NAME);
      setSchema(SOAP_SCHEMA);
      setHavingClassTag("jboss-net:web-service");
      setValidateXML(false);
      setSubTaskName(SUBTASK_NAME);
   }

   protected abstract String getTemplateName();
   
   public String getWebDeploymentName() {
      return applicationName;
   }

   public String getTargetNameSpace() {
      return this.targetNameSpace;
   }

   public String getPrefix() {
      return this.prefix;
   }

   public void setWebDeploymentName(String name) {
      this.applicationName= name;
   }

   public void setTargetNameSpace(String name) {
      this.targetNameSpace= name;
   }

   public void setPrefix(String prefix) {
      this.prefix= prefix;
   }

   public void engineStarted() {
   }

}
