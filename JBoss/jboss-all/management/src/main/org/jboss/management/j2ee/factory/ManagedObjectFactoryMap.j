/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.factory;

import javax.management.Notification;
import javax.management.ObjectName;

/** A ManagedObjectFactoryMap is a collection of ManagedObjectFactorys keyed
 * by Notifications. An implementation has to be able to map from the various
 * type of events and their associated data to the ManagedObjectFactory that
 * is able the create the JSR-77 management object(s) that should be available
 * for the core JBoss component represented by the Notification.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.8 $
 */
public interface ManagedObjectFactoryMap
{
   public ManagedObjectFactory getFactory(Notification createEvent);

   /** Set the ObjectName to recognize as the SARDeployer component
    * @param name the SARDeployer name
    */
   public void setSARDeployer(ObjectName name);
   /** Set the ObjectName to recognize as the EARDeployer component
    * @param name the EARDeployer name
    */
   public void setEARDeployer(ObjectName name);
   /** Set the ObjectName to recognize as the EJBDeployer component
    * @param name the EJBDeployer name
    */
   public void setEJBDeployer(ObjectName name);
   /** Set the ObjectName to recognize as the RARDeployer component
    * @param name the RARDeployer name
    */
   public void setRARDeployer(ObjectName name);
   /** Set the ObjectName to recognize as the JCA Connection manager deployer component
    * @param name the JCA Connection manager deployer name
    */
   public void setCMDeployer(ObjectName name);

   /** Set the ObjectName to recognize as the WARDeployer component
    * @param name the WARDeployer name
    */
   public void setWARDeployer(ObjectName name);

   /** Set the ObjectName to recognize as a JavaMail resource component
    * @param name the JavaMail service name
    */
   public void setJavaMailResource(ObjectName name);
   /** Set the ObjectName to recognize as a JMS resource component
    * @param name the JMS service name
    */
   public void setJMSResource(ObjectName name);
   /** Set the ObjectName to recognize as a JNDI resource component
    * @param name the JNDI service name
    */
   public void setJNDIResource(ObjectName name);
   /** Set the ObjectName to recognize as a JTA resource component
    * @param name the JTA service name
    */
   public void setJTAResource(ObjectName name);

/** Set the ObjectName to recognize as a RMI_IIOP resource component
    * @param name the RMI_IIOP service name
    */
   public void setRMI_IIOPResource(ObjectName name);
}
