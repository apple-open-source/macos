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
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Set;

import javax.jms.InvalidClientIDException;
import javax.jms.JMSException;
import javax.jms.JMSSecurityException;
import javax.naming.InitialContext;

import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.server.JMSDestinationManager;
import org.jboss.mq.xml.XElement;
import org.jboss.mq.xml.XElementException;
import org.jboss.mq.sm.StateManager;
import org.jboss.mq.server.JMSTopic;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.server.ServerConfigLocator;

/**
 * This class is a simple User Manager. It handles credential issues.
 *
 * <p>This is the old state manager.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     <a href="hiram.chirino@jboss.org">Hiram Chirino</a>
 * @author     <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version    $Revision: 1.5 $
 */
public class OldStateManager
   extends ServiceMBeanSupport
   implements StateManager, OldStateManagerMBean
{
   XElement stateConfig;

   private final Set loggedOnClientIds = new HashSet();
   private String stateFile = "conf/default/jbossmq-state.xml";
   private URL systemConfigURL;
   
   /**
    *  Constructor for the StateManager object
    *
    * @exception  XElementException  Description of Exception
    */
   public OldStateManager() throws XElementException
   {
      //loggedOnClientIds = new HashSet();
   }

   protected void createService() throws Exception {
      // Get the system configuration URL
      systemConfigURL = ServerConfigLocator.locate().getServerConfigURL();
   }
   
   /**
    * Sets the StateFile attribute of the StateManagerMBean object
    *
    * @jmx:managed-attribute
    *
    * @param newStateFile    The new StateFile value
    */
   public void setStateFile(String newStateFile)
   {
      stateFile = newStateFile.trim();
   }

   /**
    * @jmx:managed-attribute
    */
   public StateManager getInstance()
   {
      return this;
   }

   /**
    *  Sets the DurableSubscription attribute of the StateManager object
    *
    * @param  server            The new DurableSubscription value
    * @param  sub               The new DurableSubscription value
    * @param  topic             The new DurableSubscription value
    * @exception  JMSException  Description of Exception
    */
   public void setDurableSubscription(JMSDestinationManager server, DurableSubscriptionID sub, SpyTopic topic)
      throws JMSException
   {
      boolean debug = log.isDebugEnabled();
      if (debug)
         log.debug("Checking durable subscription: " + sub + ", on topic: " + topic);
      try
      {
         //Set the known Ids
         Enumeration enum = stateConfig.getElementsNamed("User");
         while (enum.hasMoreElements())
         {

            // Match the User.Name
            XElement user = (XElement)enum.nextElement();
            if (!user.containsField("Id") || !user.getField("Id").equals(sub.getClientID()))
            {
               continue;
            }

            if (debug)
               log.debug("Found a matching ClientID configuration section.");

            XElement subscription = null;

            // Match the User/DurableSubscription.Name
            Enumeration enum2 = user.getElementsNamed("DurableSubscription");
            while (enum2.hasMoreElements())
            {
               XElement t = (XElement)enum2.nextElement();
               if (t.getField("Name").equals(sub.getSubscriptionName()))
               {
                  subscription = t;
                  break;
               }
            }

            // A new subscription
            if (subscription == null)
            {
               if (debug)
                  log.debug("The subscription was not previously registered.");
               // it was not previously registered...
               if (topic == null)
               {
                  return;
               }

               JMSTopic dest = (JMSTopic)server.getJMSDestination(topic);
               dest.createDurableSubscription(sub);

               subscription = new XElement("DurableSubscription");
               subscription.addField("Name", sub.getSubscriptionName());
               subscription.addField("TopicName", topic.getName());
               user.addElement(subscription);

               saveConfig();

            }
            // An existing subscription...it was previously registered...
            // Check if it is an unsubscribe, or a change of subscription.
            else
            {
               if (debug)
                  log.debug("The subscription was previously registered.");
               
               // The client wants an unsubscribe 
               // TODO: we are not spec compliant since we neither check if
               // the topic has an active subscriber or if there are messages
               // destined for the client not yet acked by the session!!!
               if (topic == null)
               {
                  // we have to change the subscription...
                  SpyTopic prevTopic = new SpyTopic(subscription.getField("TopicName"));
                  JMSTopic dest = (JMSTopic)server.getJMSDestination(prevTopic);
                  // TODO here we should check if  the client still has 
                  // an active consumer

                  dest.destroyDurableSubscription(sub);

                  //straight deletion, remove subscription
                  subscription.removeFromParent();
                  saveConfig();
               }
               // The topic previously subscribed to is not the same as the
               // one in the subscription request.
               // TODO: we do currently no check if the selector has changed.
               else if (!subscription.getField("TopicName").equals(topic.getName()))
               {
                  //new topic so we have to change it
                  if (debug)
                     log.debug("But the topic was different, changing the subscription.");
                  // we have to change the subscription...
                  SpyTopic prevTopic = new SpyTopic(subscription.getField("TopicName"));
                  JMSTopic dest = (JMSTopic)server.getJMSDestination(prevTopic);
                  dest.destroyDurableSubscription(sub);

                  dest = (JMSTopic)server.getJMSDestination(topic);
                  dest.createDurableSubscription(sub);

                  //durable subscription has new topic
                  subscription.setField("TopicName", topic.getName());
                  saveConfig();
               }
            }
            return;
         }

         // Could not find that user..
         throw new JMSException("ClientID '" + sub.getClientID() +
                                "' cannot create durable subscriptions.");
      }
      catch (IOException e)
      {
         JMSException newE = new SpyJMSException("Could not setup the durable subscription");
         newE.setLinkedException(e);
         throw newE;
      }
      catch (XElementException e)
      {
         JMSException newE = new SpyJMSException("Could not setup the durable subscription");
         newE.setLinkedException(e);
         throw newE;
      }

   }
   
   /**
    * Get the destination a subscription is for.
    */
   public SpyTopic getDurableTopic(DurableSubscriptionID sub) throws JMSException
   {
      try {
         XElement subscription = getSubscription(sub);
         if (subscription == null)
            throw new JMSException("No durable subscription found for subscription: " + sub.getSubscriptionName());
         
         return new SpyTopic(subscription.getField("TopicName"));
      }catch(XElementException e)
      {
         JMSException newE = new SpyJMSException("Could not find durable subscription");
         newE.setLinkedException(e);
         throw newE;
      }
   }
   
   private XElement getSubscription(DurableSubscriptionID sub) throws JMSException,XElementException {
      boolean debug = log.isDebugEnabled();

      //Set the known Ids
      XElement subscription = null;
      Enumeration enum = stateConfig.getElementsNamed("User");
      while (enum.hasMoreElements())
      {
            
         // Match the User.Name
         XElement user = (XElement)enum.nextElement();
         if (!user.containsField("Id") || !user.getField("Id").equals(sub.getClientID()))
         {
            continue;
         }

         if (debug)
            log.debug("Found a matching ClientID configuration section.");

         //XElement subscription = null;

         // Match the User/DurableSubscription.Name
         Enumeration enum2 = user.getElementsNamed("DurableSubscription");
         while (enum2.hasMoreElements())
         {
            XElement t = (XElement)enum2.nextElement();
            if (t.getField("Name").equals(sub.getSubscriptionName()))
            {
               subscription = t;
               //break;
               return subscription;
            }
         }
      }
      // Nothing found  will be null
      return subscription;
   }

   /**
    * Gets the StateFile attribute of the StateManagerMBean object
    *
    * @jmx:managed-attribute
    *
    * @return    The StateFile value
    */
   public String getStateFile()
   {
      return stateFile;
   }

   /**
    *  #Description of the Method
    *
    * @param  login             Description of Parameter
    * @param  passwd            Description of Parameter
    * @return                   Description of the Returned Value
    * @exception  JMSException  Description of Exception
    */
   public String checkUser(String login, String passwd) throws JMSException
   {
      try
      {
         synchronized (stateConfig)
         {

            Enumeration enum = stateConfig.getElementsNamed("User");
            while (enum.hasMoreElements())
            {
               XElement element = (XElement)enum.nextElement();
               String name = element.getField("Name");
               if (!name.equals(login))
               {
                  continue;
               }

               String pw = element.getField("Password");
               if (!passwd.equals(pw))
               {
                  throw new JMSException("Bad password");
               }

               String clientId = null;
               if (element.containsField("Id"))
               {
                  clientId = element.getField("Id");
               }

               if (clientId != null)
               {
                  synchronized (loggedOnClientIds)
                  {
                     if (loggedOnClientIds.contains(clientId))
                     {
                        throw new JMSSecurityException
                           ("The login id has an assigned client id. " +
                            "That client id is already connected to the server!");
                     }
                     loggedOnClientIds.add(clientId);
                  }
               }

               return clientId;
            }
            throw new JMSSecurityException("This user does not exist");
         }
      }
      catch (XElementException e)
      {
         log.error(e);
         throw new JMSException("Invalid server user configuration.");
      }
   }

   /**
    *  Ad a logged in clientID to the statemanager.
    *
    * The clientID must not be active, and it must not be password protected.
    *
    * @param  ID                The feature to be added to the LoggedOnClientId
    *                           attribute
    * @exception  JMSException  Description of Exception
    */
   public void addLoggedOnClientId(String ID) throws JMSException
   {

      //Check : this ID must not be registered
      synchronized (loggedOnClientIds)
      {
         if (loggedOnClientIds.contains(ID))
         {
            throw new InvalidClientIDException("This loggedOnClientIds is already registered !");
         }
      }

      //Check : this ID must not be password protected
      synchronized (stateConfig)
      {
         Enumeration enum = stateConfig.getElementsNamed("User");
         while (enum.hasMoreElements())
         {
            XElement element = (XElement)enum.nextElement();
            try
            {
               if (element.containsField("Id") && element.getField("Id").equals(ID))
               {
                  throw new InvalidClientIDException("This loggedOnClientIds is password protected !");
               }
            }
            catch (XElementException ignore)
            {
            }
         }

      }
      synchronized (loggedOnClientIds)
      {
         loggedOnClientIds.add(ID);
      }
   }

   /**
    *  #Description of the Method
    *
    * @param  ID  Description of Parameter
    */
   public void removeLoggedOnClientId(String ID)
   {
      synchronized (loggedOnClientIds)
      {
         loggedOnClientIds.remove(ID);
      }
   }

   /**
    *  #Description of the Method
    *
    * @exception  Exception  Description of Exception
    */
   public void startService() throws Exception
   {

      loadConfig();
   }

   public Collection getDurableSubscriptionIdsForTopic(SpyTopic topic)
      throws JMSException
   {
      Collection durableSubs = new ArrayList();
      try 
      {
         Enumeration enum = stateConfig.getElementsNamed("User/DurableSubscription");
         while (enum.hasMoreElements())
         {
            XElement element = (XElement)enum.nextElement();

            String clientId = element.getField("../Id");
            String name = element.getField("Name");
            String topicName = element.getField("TopicName");
            if (topic.getName().equals(topicName)) 
            {
               durableSubs.add(new DurableSubscriptionID(clientId, name, null));
            } // end of if ()
            
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
   
   /**
    *  #Description of the Method
    *
    * @param  server                                  Description of Parameter
    * @exception  XElementException  Description of Exception
    */
   /*
     public void initDurableSubscriptions(JMSServer server) throws XElementException
     {

     //Set the known Ids
     Enumeration enum = stateConfig.getElementsNamed("User/DurableSubscription");
     while (enum.hasMoreElements())
     {
     XElement element = (XElement)enum.nextElement();

     String clientId = element.getField("../Id");
     String name = element.getField("Name");
     String topicName = element.getField("TopicName");

     try
     {

     log.debug("Restarting Durable Subscription: " + clientId + "," + name + "," + topicName);
     SpyTopic topic = new SpyTopic(topicName);
     JMSTopic dest = (JMSTopic)server.getJMSDestination(topic);
     if (dest == null)
     {
     log.warn("Subscription topic of not found: " + topicName);
     log.warn("Subscription cannot be initialized: " + clientId + "," + name);
     element.removeFromParent();
     }
     else
     {
     dest.createDurableSubscription(new DurableSubscriptionID(clientId, name));
     }

     }
     catch (JMSException e)
     {
     log.error("Could not initialize a durable subscription for : Client Id=" + clientId + ", Name=" + name + ", Topic Name=" + topicName, e);
     }
     }

     }
   */
   
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
         stateConfig = XElement.createFrom(in);
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
            stream.print(stateConfig.toXML(true));
         }
         finally {
            stream.close();
         }
      }
      else {
         log.error("Can not save configuration to non-file URL: " + configURL);
      }
   }

   /**
    * @jmx:managed-operation
    */
   public String displayStateConfig() throws Exception
   {
      return stateConfig.toString();
   }

   /*
     Does not seem to be used
   public class Identity
   {
      String login;
      String passwd;
      String loggedOnClientIds;

      Identity(final String login,
               final String passwd,
               final String loggedOnClientIds)
      {
         this.login = login;
         this.passwd = passwd;
         this.loggedOnClientIds = loggedOnClientIds;
      }

   }
   */
}
