/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import javax.resource.spi.ConnectionRequestInfo;

import javax.jms.Session;

import org.jboss.util.Strings;

/**
 * ???
 *
 * Created: Thu Mar 29 16:29:55 2001
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @version $Revision: 1.2 $
 */
public class JmsConnectionRequestInfo
   implements ConnectionRequestInfo
{
   private String userName;
   private String password;

   private boolean transacted = true;
   private int acknowledgeMode = Session.AUTO_ACKNOWLEDGE;
   private boolean isTopic = true;

   /**
    * Creats with the MCF configured properties.
    */
   public JmsConnectionRequestInfo(JmsMCFProperties prop)
   {
      this.userName = prop.getUserName();
      this.password = prop.getPassword();
      this.isTopic = prop.isTopic();
   }

   /**
    * Create with specified properties.
    */
   public JmsConnectionRequestInfo(final boolean transacted, 
				   final int acknowledgeMode,
				   final boolean isTopic)
   {
      this.transacted = transacted;
      this.acknowledgeMode = acknowledgeMode;
      this.isTopic = isTopic;
   }
   
   /**
    * Fill in default values if missing. Only applies to user and password.
    */
   public void setDefaults(JmsMCFProperties prop)
   {
      if (userName == null)
	 userName = prop.getUserName();//May be null there to
      if (password == null) 
	 password = prop.getPassword();//May be null there to
   }

   public String getUserName() 
   {
      return userName;
   }
    
   public void setUserName(String name) 
   {
      userName = name;
   }

   public String getPassword() 
   {
      return password;
   }

   public void setPassword(String password) 
   {
      this.password = password;
   }

   public boolean isTransacted()
   {
      return transacted;
   }
    
   public int getAcknowledgeMode()
   {
      return acknowledgeMode;
   }

   public boolean isTopic() {
      return isTopic;
   }

   public boolean equals(Object obj) {
      if (obj == null) return false;
      if (obj instanceof JmsConnectionRequestInfo)
      {
	 JmsConnectionRequestInfo you = (JmsConnectionRequestInfo) obj;
	 return (this.transacted == you.isTransacted() &&
		 this.acknowledgeMode == you.getAcknowledgeMode() &&
		 this.isTopic == you.isTopic() &&
		 Strings.compare(userName, you.getUserName()) &&
		 Strings.compare(password, you.getPassword()));
      }
      else {
	 return false;
      }
   }
 
   // FIXME !!
   public int hashCode() {
      String result = "" + userName + password + transacted + acknowledgeMode + isTopic;
      return result.hashCode();
   }
    
   /**
    * May be used if we fill in username and password later.
    */
   private boolean isEqual(Object o1, Object o2) {
      if (o1 == null) {
	 return (o2 == null);
      } else {
	 return o1.equals(o2);
      }
   }
}
