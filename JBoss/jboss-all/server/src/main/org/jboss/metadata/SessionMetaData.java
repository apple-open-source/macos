/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import java.util.HashMap;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;

import org.jboss.deployment.DeploymentException;

/** The meta data information specific to session beans.
 *
 *   @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *   @author <a href="mailto:Scott_Stark@displayscape.com">Scott Stark</a>.
 *   @version $Revision: 1.14 $
 */
public class SessionMetaData extends BeanMetaData 
{
   // Constants -----------------------------------------------------
   public static final String DEFAULT_STATEFUL_INVOKER = "stateful-rmi-invoker";
   public static final String DEFAULT_CLUSTERED_STATEFUL_INVOKER = "clustered-stateful-rmi-invoker";
   public static final String DEFAULT_STATELESS_INVOKER = "stateless-rmi-invoker";
   public static final String DEFAULT_CLUSTERED_STATELESS_INVOKER = "clustered-stateless-rmi-invoker";
    
   // Attributes ----------------------------------------------------
   private boolean stateful;

   // Static --------------------------------------------------------
    
   // Constructors --------------------------------------------------
   public SessionMetaData(ApplicationMetaData app)
   {
      super(app, BeanMetaData.SESSION_TYPE);
   }
	
   // Public --------------------------------------------------------
   public boolean isStateful() { return stateful; }
   public boolean isStateless() { return !stateful; }
		
   public String getDefaultConfigurationName()
   {
      if (isStateful()) {
         if (this.isClustered())
            return ConfigurationMetaData.CLUSTERED_STATEFUL_13;
         else
            return ConfigurationMetaData.STATEFUL_13;
      } else {
         if (this.isClustered())
            return ConfigurationMetaData.CLUSTERED_STATELESS_13;
         else
            return ConfigurationMetaData.STATELESS_13;
      }
   }
	

   protected void defaultInvokerBindings()
   {
      invokerBindings = new HashMap();
      if (isClustered())
      {
         if (stateful)
         {
            invokerBindings.put(DEFAULT_CLUSTERED_STATEFUL_INVOKER, getJndiName());
         }
         else
         {
            invokerBindings.put(DEFAULT_CLUSTERED_STATELESS_INVOKER, getJndiName());
         }
      }
      else
      {
         if (stateful)
         {
            invokerBindings.put(DEFAULT_STATEFUL_INVOKER, getJndiName());
         }
         else
         {
            invokerBindings.put(DEFAULT_STATELESS_INVOKER, getJndiName());
         }
      }
   }

   public void importEjbJarXml(Element element) throws DeploymentException {
      super.importEjbJarXml(element);
		
      // set the session type 
      String sessionType = getElementContent(getUniqueChild(element, "session-type"));
      if (sessionType.equals("Stateful")) {
         stateful = true;
      } else if (sessionType.equals("Stateless")) {
         stateful = false;
      } else {
         throw new DeploymentException("session type should be 'Stateful' or 'Stateless'");
      }
			
      // set the transaction type
      String transactionType = getElementContent(getUniqueChild(element, "transaction-type"));
      if (transactionType.equals("Bean")) {
         containerManagedTx = false;
      } else if (transactionType.equals("Container")) {
         containerManagedTx = true;
      } else {
         throw new DeploymentException("transaction type should be 'Bean' or 'Container'");
      }
   }

   // Package protected ---------------------------------------------
    
   // Protected -----------------------------------------------------
    
   // Private -------------------------------------------------------
    
   // Inner classes -------------------------------------------------
}
