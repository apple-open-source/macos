/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.mq.security;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import javax.jms.Destination;
import javax.jms.JMSException;
import javax.jms.JMSSecurityException;
import javax.jms.InvalidDestinationException;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;

import org.jboss.mq.ConnectionToken;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.Subscription;
import org.jboss.mq.server.JMSServerInterceptorSupport;
/**
 * ServerSecurityInvoker.java
 *
 * @author <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1.4.1 $
 */

public class ServerSecurityInterceptor extends JMSServerInterceptorSupport
{
   SecurityManager manager;

   /**
    * The temporary destinations for a connection
    */
   private HashMap tempDests = new HashMap();

   public ServerSecurityInterceptor(SecurityManager manager) {
      super();
      this.manager = manager;
   }
   
   public String authenticate(String name, String password) throws JMSException {
      log.trace("Autenticating user " +name +"/"+password);
      return manager.authenticate(name,password);
   }
   /**
    * Close connection. Logout user after connection is closed.
    *
    * @param dc             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void connectionClosing(ConnectionToken dc)
      throws JMSException {
      super.connectionClosing(dc);
      manager.logout(dc);
      removeTemporaryDestinations(dc);
   }
   /*
     Here is a number of methods that I do no know if we should check access on.
     
     createQueue()
     createTopic()

     unsubscribe() - probably not.
    */

   //
   // Read methods, to check access on
   // 
   public SpyMessage[] browse(ConnectionToken dc, Destination dest, String selector)
      throws JMSException {
      if (log.isTraceEnabled())
         log.trace( "Checking browse authorize on " +dc + " dest=" +dest);

      if (!authorizeRead(dc, ((SpyDestination)dest).getName()) )
         throw new JMSSecurityException("Connection not autorized to browse to destination: " + dest);

      return super.browse(dc,dest,selector);
   }

   // FIXME This might actually be unneeded since a subscribe is 
   // allways done first. 
   // fuck.
   // If we remove it, remember to remove the getSubscription method
   public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait)
      throws JMSException {
      if (log.isTraceEnabled())
         log.trace( "Checking receive authorize on " +dc + " subId="+subscriberId);
      // Another nightmare, how the fck do we get the dest name.
      Subscription sub = super.getSubscription(dc,subscriberId);
      String destName = sub.destination.getName();
      if (!authorizeRead(dc, destName) )
         throw new JMSSecurityException("Connection not autorized to receive from destination: " + destName);


      return super.receive(dc,subscriberId,wait);
   }
   
   // The price we pay for adding this to an implementation not done
   // for acl's. The method to create a durable susbcriber is this, but
   // with a durableSubscriptionID in the destination of the subscription.
   // For all but, durable subscriptions this is a read access thingy.
   // Even more: if this is a create of a durable sub, or a change of one
   // or just usage of an existing one, that does basically only JMSTopic (it's new or has changed) and StateManager (its allowed) know of.
   // The logic has to be this, to not get into trouble: for ALL usage
   // of a durable sub create access is demanded!
   //
   public void subscribe(org.jboss.mq.ConnectionToken dc, org.jboss.mq.Subscription sub)
      throws JMSException {
      
      if (log.isTraceEnabled())
         log.trace( "Checking subscribe authorize on " + dc +" sub="+sub);

      // Do some sanity checks
      if (sub == null)
         throw new JMSException("The subscription is not allowed to be null");
      else if (sub.destination == null)
         throw new InvalidDestinationException ("Destination is not allowed to be null");
      
      // Check if its a durable sub/ this might actually not be true create
      // only access to an old one. But we only allow read from durable
      // if you actually have the rights to create one, or...does this make
      // preconfigured clientID meaningless.
      
      SpyDestination dest = sub.destination;
      String destName = dest.getName();
      if (dest instanceof SpyTopic) {
            // Check durable sub
            DurableSubscriptionID id = ((SpyTopic)dest).
               getDurableSubscriptionID();

            if (id != null) {
               // Durable sub, check create access.
               if (!authorizeCreate(dc, destName))
                  throw new JMSSecurityException("Connection not authorized to do durable subscription on topic: " + destName);
            }
            
      }
      // We ALLWAYS check read access, even for durables
      if(!authorizeRead(dc,destName))
         throw new JMSSecurityException("Connection not authorized to subscribe to destination: " + destName);
      
      super.subscribe(dc,sub);
   }

   //
   // Write methods, to check access on
   //
   public void addMessage(ConnectionToken dc, SpyMessage message)
      throws JMSException {
      String dest = ((SpyDestination)message.getJMSDestination()).getName();
      if (!authorizeWrite(dc, dest))
         throw new JMSSecurityException("Connection not autorized to addMessages to destination: " + dest);
      
      super.addMessage(dc,message);
   }

   //
   // Create methods, to check access on
   //
   public void destroySubscription(ConnectionToken dc,DurableSubscriptionID id)
      throws JMSException {
      // Oh, this fucker is a nightmare. How do we get wich topic the
      // connection is trying to unsubscribe from
      SpyTopic t = super.getDurableTopic(id);
      if (t == null)
         throw new InvalidDestinationException("No durable topic found for subscription " +  id.getSubscriptionName());

      if (!authorizeCreate(dc,t.getName()))
         throw new JMSSecurityException("Connection not autorized to unsubscribe from subscription: " + t.getName());
      
      super.destroySubscription(dc,id);
   }

   public TemporaryTopic getTemporaryTopic(ConnectionToken dc)
      throws JMSException
   {
      TemporaryTopic result = super.getTemporaryTopic(dc);
      addTemporaryDestination(dc, result);
      return result;
   }

   public TemporaryQueue getTemporaryQueue(ConnectionToken dc)
      throws JMSException
   {
      TemporaryQueue result = super.getTemporaryQueue(dc);
      addTemporaryDestination(dc, result);
      return result;
   }

   public void deleteTemporaryDestination(ConnectionToken dc, SpyDestination destination)
      throws JMSException
   {
      removeTemporaryDestination(dc, destination);
      super.deleteTemporaryDestination(dc, destination);
   }   

   //
   // Security helper methods
   //
   public boolean authorizeRead(ConnectionToken dc,String destination) throws JMSException{
      // First we must get access to the destinations security meta data
      SecurityMetadata m = manager.getSecurityMetadata(destination);
      if (m == null) {
         log.warn("No security configuration avaliable for " + destination);
         return false;//FIXME, is this OK?
      }
      Set readPrincipals = m.getReadPrincipals();
      if (manager.authorize(dc,readPrincipals))
         return true;
      else
         return false;
      
   }

   public boolean authorizeWrite(ConnectionToken dc,String destination) throws JMSException{
      // First we must get access to the destinations security meta data
      SecurityMetadata m = manager.getSecurityMetadata(destination);
      if (m == null) {
         log.warn("No security configuration avaliable for " + destination);
         return false;//FIXME, is this OK?
      }
      Set writePrincipals = m.getWritePrincipals();
      if (manager.authorize(dc,writePrincipals))
         return true;
      else
         return false;
      
   }
   
   public boolean authorizeCreate(ConnectionToken dc,String destination) throws JMSException{
      // First we must get access to the destinations security meta data
      SecurityMetadata m = manager.getSecurityMetadata(destination);
      if (m == null) {
         log.warn("No security configuration avaliable for " + destination);
         return false;//FIXME, is this OK?
      }
      Set createPrincipals = m.getCreatePrincipals();
      if (manager.authorize(dc,createPrincipals))
         return true;
      else
         return false;
      
   }

   /**
    * Remember the temporary destinations for a connection
    */
   public void addTemporaryDestination(ConnectionToken dc, Destination destination)
   {
      synchronized(tempDests)
      {
         HashSet set = (HashSet) tempDests.get(dc);
         if (set == null)
         {
            set = new HashSet();
            tempDests.put(dc, set);
         }
         set.add(destination);
      }
   }

   /**
    * Remove a temporary destination
    */
   public void removeTemporaryDestination(ConnectionToken dc, SpyDestination destination)
   {
      synchronized(tempDests)
      {
         HashSet set = (HashSet) tempDests.get(dc);
         if (set == null)
            return;
         set.remove(destination);
      }
      try
      {
         manager.removeDestination(destination.getName());
      }
      catch (Exception e)
      {
         log.warn("Unable to remove temporary destination " + destination, e);
      }
   }

   /**
    * Remove all temporary destination for a connection
    */
   public void removeTemporaryDestinations(ConnectionToken dc)
   {
      synchronized(tempDests)
      {
         HashSet set = (HashSet) tempDests.remove(dc);
         if (set == null)
            return;
         for (Iterator iterator = set.iterator(); iterator.hasNext();)
         {
            SpyDestination destination = (SpyDestination) iterator.next();
            try
            {
               manager.removeDestination(destination.getName());
            }
            catch (Exception e)
            {
               log.warn("Unable to remove temporary destination " + destination, e);
            }
         }
      }
   }
} // ServerSecurityInvoker




