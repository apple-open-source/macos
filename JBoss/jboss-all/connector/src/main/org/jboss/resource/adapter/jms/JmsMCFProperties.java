/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import javax.resource.ResourceException;

import org.jboss.util.Strings;

/**
 * The MCF default properties, settable in ra.xml or in deployer.
 *
 * Created: Thu Sep 27 10:01:25 2001
 *
 * @author Peter Antman
 * @version $Revision: 1.2 $
 */
public class JmsMCFProperties
   implements java.io.Serializable
{
   public static final String QUEUE_TYPE = javax.jms.Queue.class.getName();
   public static final String TOPIC_TYPE = javax.jms.Topic.class.getName();

   String userName;
   String password;
   String providerJNDI = "java:DefaultJMSProvider";
   boolean isTopic = true;
   
   public JmsMCFProperties() {
      // empty
   }
   
   /**
    * Set userName, null by default.
    */
   public void setUserName(final String userName) {
      this.userName = userName;
   }

   /**
    * Get userName, may be null.
    */ 
   public String getUserName() {
      return userName;
   }
   
   /**
    * Set password, null by default.
    */
   public void setPassword(final String password) {
      this.password = password;
   }
   
   /**
    * Get password, may be null.
    */
   public String getPassword() {
      return password;
   }

   /**
    * Set providerJNDI, the JMS provider adapter to use.
    *
    * <p>Defaults to java:DefaultJMSProvider.
    */
   public void setProviderJNDI(final String providerJNDI) {
      this.providerJNDI  = providerJNDI;
   }

   /**
    * Get providerJNDI. May not be null.
    */
   public String getProviderJNDI() {
      return providerJNDI;
   }

   /**
    * Type of the JMS Session, defaults to true.
    */
   public boolean isTopic() {
      return isTopic;
   }

   /**
    * Set the default session type.
    */
   public void setIsTopic(boolean isTopic) {
      this.isTopic = isTopic;
   }

   /**
    * Helper method to set the default session type.
    *
    * @param type either javax.jms.Topic or javax.jms.Queue
    * @exception ResourceException if type was not a valid type.
    */
   public void setSessionDefaultType(String type) throws ResourceException
   {
      if (type.equals(QUEUE_TYPE)) {
	 isTopic = false;
      }
      else if(type.equals(TOPIC_TYPE)) {
	 isTopic = true;
      }
      else {
	 throw new  ResourceException(type + " is not a recogniced JMS session type");
      }
   }

   public String getSessionDefaultType() {
      return (isTopic ? TOPIC_TYPE : QUEUE_TYPE);
   }

   /**
    * Test for equality of all attributes.
    */
   public boolean equals(Object obj) {
      if (obj == null) return false;
      
      if (obj instanceof JmsMCFProperties) {
         JmsMCFProperties you = (JmsMCFProperties) obj;
         return (Strings.compare(userName, you.getUserName()) &&
                 Strings.compare(password, you.getPassword()) &&
                 Strings.compare(providerJNDI, you.getProviderJNDI()) &&
                 this.isTopic == you.isTopic());
      }
      
      return false;
   }
 
   /**
    * Simple hashCode of all attributes. 
    */
   public int hashCode() {
      // FIXME
      String result = "" + userName + password + providerJNDI + isTopic;
      return result.hashCode();
   }
}
