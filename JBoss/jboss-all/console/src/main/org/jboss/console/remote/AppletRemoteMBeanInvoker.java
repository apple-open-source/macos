/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.remote;
/**
 * <description>
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

public class AppletRemoteMBeanInvoker
implements SimpleRemoteMBeanInvoker
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   java.net.URL baseUrl = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public AppletRemoteMBeanInvoker (String baseUrl) throws java.net.MalformedURLException
   {
      this.baseUrl = new java.net.URL (baseUrl);
   }
   
   // Public --------------------------------------------------------
   
   // SimpleRemoteMBeanInvoker implementation ----------------------------------------------
   
   public Object invoke (javax.management.ObjectName name, String operationName, Object[] params, String[] signature) throws Exception
   {
      return Util.invoke (this.baseUrl, new RemoteMBeanInvocation (name, operationName, params, signature));
   }

   public Object getAttribute (javax.management.ObjectName name, String attrName) throws Exception
   {
      return Util.getAttribute(this.baseUrl, new RemoteMBeanAttributeInvocation(name, attrName));
   }

   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
