/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.sm.file;

import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.io.InputStream;
import java.io.BufferedInputStream;
import java.io.IOException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Enumeration;

import javax.jms.InvalidClientIDException;
import javax.jms.JMSException;
import javax.jms.JMSSecurityException;

import org.jboss.system.server.ServerConfigLocator;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.xml.XElement;
import org.jboss.mq.xml.XElementException;

import org.jboss.mq.sm.StateManager;
import org.jboss.mq.sm.AbstractStateManager;

/**
 * A state manager that allowed durable subscriptions to be dynamically
 * created if configured to support it. Otherwise backward compatible with
 * the old StateManager.
 *
 * <p>Backed by an XML file.
 *
 * <p>Example file format:
 * <xmp>
<StateManager>
 <Users>
   <User>
    <Name>john</Name>
    <Password>needle</Password>
    <Id>DurableSubscriberExample</Id><!-- optional -->
   </User>
 </Users>

 <Roles>
  <Role name="guest">
    <UserName>john</UserName>
  </Role>

 </Roles>

 <DurableSubscriptions>
   <DurableSubscription>
     <ClientID>needle</ClientID>
     <Name>myDurableSub</Name>
     <TopicName>TestTopic...</TopicName>
   </DurableSubscription>

 </DurableSubscriptions>
</StateManager>
 * </xmp>
 *
 * @jmx:mbean extends="org.jboss.mq.sm.AbstractStateManagerMBean"
 *
 * @author     <a href="Norbert.Lataille@m4x.org">Norbert Lataille</a>
 * @author     <a href="hiram.chirino@jboss.org">Hiram Chirino</a>
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.4.2.1 $
 */
public class DynamicStateManager 
   extends AbstractStateManager
   implements DynamicStateManagerMBean
{
   class DynamicDurableSubscription extends DurableSubscription {
      XElement element;
      public  DynamicDurableSubscription(XElement element) throws XElementException{
         super(element.getField("ClientID"),
              element.getField("Name"),
              element.getField("TopicName"),
              element.getOptionalField("Selector")
              );
         this.element = element;
      }
      XElement getElement() { return element; }
      
      // Hm, I don't think we should mutate it without a sync.
      //public void setTopic(String topic) { 
      //   this.topic = topic;
      //   element.setField("Topic", topic);
      // }
      
   }

   /**
    * Do we have a security manager.
    *
    * By setting this to false, we may emulate the old behaviour of
    * the state manager and let it autenticate connections.
    */
   boolean hasSecurityManager = true; 
   
   XElement stateConfig = new XElement("StateManager");//So sync allways work

   /** State file is relateive to systemConfigURL. */
   private String stateFile = "jbossmq-state.xml";
   private URL systemConfigURL;
   
   
   public DynamicStateManager() {
      
   }
   //
   // MBean methods
   //
   public StateManager getInstance()
   {
      return this;
   }

   protected void createService() throws Exception 
   {
      // Get the system configuration URL
      systemConfigURL = ServerConfigLocator.locate().getServerConfigURL();
   }
   
   public void startService() throws Exception
   {
      loadConfig();
   }

   /**
    * Show the current configuration.
    */
   public String displayStateConfig() throws Exception
   {
      return stateConfig.toString();
   }

   /**
    * Set the name of the statefile.
    *
    * @jmx:managed-attribute
    *
    * @param  newStateFile  java.lang.String
    */
   public void setStateFile(String newStateFile)
   {
      stateFile = newStateFile.trim();
   }

   /**
    * Get name of file.
    *
    * @jmx:managed-attribute
    *
    * @return    java.lang.String
    */
   public String getStateFile()
   {
      return stateFile;
   }

   /**
    * @jmx:managed-attribute
    */
   public boolean hasSecurityManager() {
      return hasSecurityManager;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setHasSecurityManager(boolean hasSecurityManager) {
      this.hasSecurityManager = hasSecurityManager;
   }

   /**
    * @jmx:managed-operation
    */
   public void loadConfig() throws IOException, XElementException
   {
      URL configURL = new URL(systemConfigURL, stateFile);
      if (log.isDebugEnabled()) {
         log.debug("Loading config from: " + configURL);
      }
      
      InputStream in = new BufferedInputStream(configURL.openStream());
      try {
         synchronized(stateConfig) {
            stateConfig = XElement.createFrom(in);
         }
      }
      finally {
         in.close();
      }
   }

   /**
    * @jmx:managed-operation
    */
   public void saveConfig() throws IOException
   {
      URL configURL = new URL(systemConfigURL, stateFile);
      
      if (configURL.getProtocol().equals("file")) {
         File file = new File(configURL.getFile());
         if (log.isDebugEnabled()) {
            log.debug("Saving config to: " + file);
         }

         PrintStream stream = new PrintStream(new FileOutputStream(file));
         try {
            synchronized(stateConfig) {
               stream.print(stateConfig.toXML(true));
            }
         }
         finally {
            stream.close();
         }
      }
      else {
         log.error("Can not save configuration to non-file URL: " + configURL);
      }
   }
   //
   // Callback methods from AbstractStateManager
   //
   /**
    * Return preconfigured client id. Only if hasSecurityManager is false will
    * a password be required to get the clientID and will the method throw
    * a JMSSecurityException if the clientID was not found.
    */
   protected String getPreconfClientId(String login,String passwd) throws JMSException {
      try
      {
         synchronized (stateConfig)
         {
            
            Enumeration enum = stateConfig.getElementsNamed("Users/User");
            while (enum.hasMoreElements())
            {
               XElement element = (XElement)enum.nextElement();
               String name = element.getField("Name");
               if (!name.equals(login))
               {
                  continue;// until user is found
               }
               
               // Onlyn check password if we do not have a security manager
               if (!hasSecurityManager) {
                  String pw = element.getField("Password");
                  if (!passwd.equals(pw))
                  {
                     throw new JMSSecurityException("Bad password");
                  }
               }
               
               String clientId = null;
               if (element.containsField("Id"))
               {
                  clientId = element.getField("Id");
               }

               //if (clientId != null)
               return clientId;
            }
            if (!hasSecurityManager)
               throw new JMSSecurityException("This user does not exist");
            else 
               return null;
         }
      }
      catch (XElementException e)
      {
         log.error(e);
         throw new JMSException("Invalid server user configuration.");
      }
   }
   
   /**
    * Search for a configurated durable subscription.
    */ 
   protected DurableSubscription getDurableSubscription(DurableSubscriptionID sub) throws JMSException {
      boolean debug = log.isDebugEnabled();
      
      //Set the known Ids
      try {
         synchronized (stateConfig)
         {
            
            Enumeration enum = stateConfig.getElementsNamed("DurableSubscriptions/DurableSubscription");
            while (enum.hasMoreElements())
            {
               
               // Match ID
               XElement dur = (XElement)enum.nextElement();
               if (dur.containsField("ClientID") && dur.getField("ClientID").equals(sub.getClientID()))
               {
                  // Check if this one has a DurableSubname that match
                  if (dur.getField("Name").equals(sub.getSubscriptionName())) {
                     // We have a match
                     if (debug)
                        log.debug("Found a matching ClientID configuration section.");
                     return new DynamicDurableSubscription(dur);
                  }
                  
                  
               }            
            }
            // Nothing found
            return null;
         }
      }catch(XElementException e)
      {
         JMSException newE = new SpyJMSException("Could not find durable subscription");
         newE.setLinkedException(e);
         throw newE;
      }
   }
   
   /**
    * Check if the clientID belonges to a preconfigured user. If this
    * is the case, a  InvalidClientIDException  will be raised.
    */
   protected void checkLoggedOnClientId(String clientID) throws JMSException {
         synchronized (stateConfig)
         {
            
            Enumeration enum = stateConfig.getElementsNamed("Users/User");
            while (enum.hasMoreElements())
            {
               XElement element = (XElement)enum.nextElement();
               try
               {
                  if (element.containsField("Id") && element.getField("Id").equals(clientID))
                  {
                     throw new InvalidClientIDException("This loggedOnClientIds is password protected !");
                  }
               }
               catch (XElementException ignore)
               {
               }
            }
         }
         
   }
   protected void saveDurableSubscription(DurableSubscription ds) throws JMSException {
      try {
         synchronized (stateConfig)
         {
            // Or logic here is simply this, if we get a DynamicDurableSubscription
            // Its reconfiguration, if not it is new
            if (ds instanceof DynamicDurableSubscription) {
               XElement s = ((DynamicDurableSubscription)ds).getElement();
               if (s != null) {
                  s.setField("TopicName",ds.getTopic());//In case it changed.
                  s.setOptionalField("Selector",ds.getSelector());//In case it changed.
               }else{
                  throw new JMSException("Can not save a null subscription");
               }
            } else {
               XElement dur = stateConfig.getElement("DurableSubscriptions");
               XElement  subscription=  new XElement("DurableSubscription");
               subscription.addField("ClientID", ds.getClientID());
               subscription.addField("Name", ds.getName());
               subscription.addField("TopicName", ds.getTopic());
               subscription.setOptionalField("Selector", ds.getSelector());
               dur.addElement(subscription);
            }
            saveConfig();
         }
      } catch (XElementException e)
      {
         JMSException newE = new SpyJMSException("Could not save the durable subscription");
          newE.setLinkedException(e);
          throw newE;
      } catch (IOException e)
      {
         JMSException newE = new SpyJMSException("Could not save the durable subscription");
          newE.setLinkedException(e);
          throw newE;
      }
   }
   protected void removeDurableSubscription(DurableSubscription ds) throws JMSException {
      try {
         // We only remove if it was our own dur sub.
         synchronized (stateConfig)
         {
            if (ds instanceof DurableSubscription) {
               XElement s = ((DynamicDurableSubscription)ds).getElement();
               if (s != null) {
                  s.removeFromParent();
                  saveConfig();
               }else{
                  throw new JMSException("Can not remove a null subscription");
               }
               
            } else {
               throw new JMSException("Only subscriptions of type DynamicDurableSubscription may be removed.");
            }
         }
      }catch (XElementException e)
      {
         JMSException newE = new SpyJMSException("Could not remove the durable subscription");
         newE.setLinkedException(e);
         throw newE;
      }catch (IOException e)
      {
         JMSException newE = new SpyJMSException("Could not remove the durable subscription");
         newE.setLinkedException(e);
         throw newE;
      }
   }
   
   public Collection getDurableSubscriptionIdsForTopic(SpyTopic topic)
      throws JMSException {
      Collection durableSubs = new ArrayList();
      try 
      {
         synchronized (stateConfig) {
            
            Enumeration enum = stateConfig.getElementsNamed("DurableSubscriptions/DurableSubscription");
            while (enum.hasMoreElements())
            {
               XElement element = (XElement)enum.nextElement();
               
               String clientId = element.getField("ClientID");
               String name = element.getField("Name");
               String topicName = element.getField("TopicName");
               String selector = element.getOptionalField("Selector");
               if (topic.getName().equals(topicName)) 
               {
                  durableSubs.add(new DurableSubscriptionID(clientId, name, selector));
               } // end of if ()
               
            }
         }
      } 
      catch (XElementException e) 
      {
         JMSException jmse = new JMSException("Error in statemanager xml");
         jmse.setLinkedException(e);
         throw jmse;
      } // end of try-catch
      return durableSubs;
   }

   //
   // The methods that allow dynamic edititing of state manager.
   //

   /**
    * @jmx:managed-operation
    */
   public void addUser(String name, String password, String preconfID) throws Exception {
      if (findUser(name) != null)
         throw new Exception("Can not add, user exist");
      
      XElement users = stateConfig.getElement("Users");
      XElement user = new XElement("User");
      user.addField("Name",name);
      user.addField("Password",password);
      if (preconfID != null )
         user.addField("Id", preconfID);      
      users.addElement(user);
      saveConfig();
   }

   /**
    * @jmx:managed-operation
    */
   public void removeUser(String name) throws Exception {
      XElement user = findUser(name);
      if (user == null)
         throw new Exception("Cant remove user that does not exist");
      
      user.removeFromParent();

      // We should also remove the user from any roles it belonges to
      String[] roles = getRoles(name);
      if (roles != null) {
         for(int i = 0; i<roles.length;i++) {
            try {
               removeUserFromRole(roles[i],name);
            }catch(Exception ex) { 
               //Just move on
            }
         }
      }

      saveConfig();
   }

   /**
    * @jmx:managed-operation
    */
   public void addRole(String name) throws Exception {
      if(findRole(name)!=null)
         throw new Exception("Cant add role, it already exists");
      
      XElement roles = stateConfig.getElement("Roles");
      XElement role = new XElement("Role");
      role.setAttribute("name",name);
      roles.addElement(role);
      saveConfig();
   }

   /**
    * @jmx:managed-operation
    */
   public void removeRole(String name) throws Exception {
      XElement role = findRole(name);
      if (role == null)
         throw new Exception("Cant remove role that does not exist");
      
      role.removeFromParent();
      saveConfig();
   }

   // FIXME; no sanity check that the "real" user does exist.
   /**
    * @jmx:managed-operation
    */
   public void addUserToRole(String roleName, String user) throws Exception {
      XElement role = findRole(roleName);
      if (role == null)
         throw new Exception("Cant add to role that does not exist");

      if (findUser(user)==null)
         throw new Exception("Cant add user to role, user does to exist");

      if (findUserInRole(role,user) != null)
         throw new Exception("Cant add user to role, user already part of role");
      // FIXME; here I am not shure how XElement work
     XElement u = new XElement("UserName");
      u.setValue(user);
      role.addElement(u);
      saveConfig();      
   }

   /**
    * @jmx:managed-operation
    */
   public void removeUserFromRole(String roleName, String user) throws Exception {
      XElement role = findRole(roleName);
      if (role == null)
         throw new Exception("Cant remove user from role that does not exist");

      XElement u = findUserInRole(role,user);
      if (u == null)
         throw new Exception("Cant remove user from role, user does not exist");
      u.removeFromParent();
      saveConfig();
            
   }

   protected XElement findUser(String user) throws Exception{
      Enumeration enum = stateConfig.getElementsNamed("Users/User");
      while (enum.hasMoreElements())
            {
               XElement element = (XElement)enum.nextElement();
               if (element.getField("Name").equals(user))
                  return element;
            }
      return null;
   }

   protected XElement findRole(String role) throws Exception{ 
      Enumeration enum = stateConfig.getElementsNamed("Roles/Role");
      while (enum.hasMoreElements())
            {
               XElement element = (XElement)enum.nextElement();
               if (element.getAttribute("name").equals(role))
                  return element;
            }
      return null;
   }
   
   protected XElement findUserInRole(XElement role, String user) throws Exception{ 
      Enumeration enum = role.getElementsNamed("UserName");
      while (enum.hasMoreElements())
            {
               XElement element = (XElement)enum.nextElement();
               if (user.equals(element.getValue()) )
                  return element;
            }
      return null;
   }
   //
   // Methods to support LoginModule
   // 
   /**
    * We currently only support one Group type Roles. The role named
    * returned should typically be put into a Roles Group principal.
    */
   public String[] getRoles(String user) throws Exception {
      ArrayList roles = new ArrayList();
      Enumeration enum = stateConfig.getElementsNamed("Roles/Role");
      while (enum.hasMoreElements()) {
         XElement element = (XElement)enum.nextElement();
         XElement u = findUserInRole(element,user);
         if (u != null)
            roles.add( element.getAttribute("name") );
      }
      return (String[]) roles.toArray( new String[roles.size()] );
   }
   
   /**
    * Validate the user/password combination. A null inputPassword will
    * allways reurn false.
    */
   public boolean validatePassword(String user, String inputPassword) throws Exception {
      boolean valid = false;
      XElement u = findUser(user);
      if (u != null) {
         String pw = u.getField("Password");
         if (inputPassword != null && inputPassword.equals(pw))
            valid = true;
      }
      return valid;
   }
   
   //
   // Helper methods
   //      

} // DynamicStateManager





