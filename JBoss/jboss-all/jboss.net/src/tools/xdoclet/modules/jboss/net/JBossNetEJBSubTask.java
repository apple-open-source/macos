/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: JBossNetEJBSubTask.java,v 1.1.2.1 2003/04/09 06:59:22 cgjung Exp $
 
package xdoclet.modules.jboss.net;

/**
 * JBossNetEJBSubTask
 * @author jung
 * @since 08.04.2003
 * @version $Revision: 1.1.2.1 $
 */

public class JBossNetEJBSubTask extends JBossNetSubTask {

   private static String DEFAULT_TEMPLATE_FILE= "xdoclet/modules/jboss/net/resources/jboss-net_ejb_xml.xdt";

   /**
    * create ejb sub task
    */
   public JBossNetEJBSubTask() {
      super();
   }

   /* (non-Javadoc)
    * @see xdoclet.modules.jboss.net.JBossNetSubTask#getTemplateName()
    */
   protected String getTemplateName() {
      return DEFAULT_TEMPLATE_FILE;
   }

}
