/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import EDU.oswego.cs.dl.util.concurrent.Semaphore;
import EDU.oswego.cs.dl.util.concurrent.ClockDaemon;
import EDU.oswego.cs.dl.util.concurrent.ThreadFactory;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.Serializable;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Properties;
import javax.jms.ConnectionMetaData;
import javax.jms.Destination;
import javax.jms.ExceptionListener;
import javax.jms.IllegalStateException;
import javax.jms.JMSException;
import javax.jms.JMSSecurityException;
import javax.jms.Queue;
import javax.jms.Topic;

import org.jboss.logging.Logger;
import org.jboss.mq.il.ClientIL;
import org.jboss.mq.il.ClientILService;
import org.jboss.mq.il.ServerIL;

/**
 * <p>This class implements javax.jms.Connection.
 * </p>
 *
 * <p>It is also the gateway through wich all calls to the JMS server is done. To
 * do its work it needs a ServerIL to invoke (@see org.jboss.mq.server.ServerIL).
 *</p>
 *
 * <p>The (new from february 2002) logic for clientID is the following:
 * if logging in with a user and passwork a preconfigured clientID may be automatically delivered from the server.
 * </p>
 *
 * <p>If the client wants to set it's own clientID it must do so on a connection
 * wich does not have a prefonfigured clientID and it must do so before it
 * calls any other methods on the connection (even getClientID()). It is not
 * allowable to use a clientID that either looks like JBossMQ internal one 
 * (beginning with ID) or a clientID that is allready in use by someone, or
 * a clientID that is already preconfigured in the server.
 *</p>
 *
 * <p>If a preconfigured ID is not get, or a valid one is not set, the server will
 * set an internal ID. This ID is NEVER possible to use for durable subscriptions.
 * If a prefconfigured ID or one manually set is possible to use to create a 
 * durable subscriptions is governed by the security configuration of JBossMQ.
 * In the default setup, only preconfigured clientID's are possible to use. 
 * If using a SecurityManager, permissions to create a surable subscriptions is * the resiult of a combination of the following:
 *</p>
 * <p>- The clientID is not one of JBossMQ's internal.
 *</p>
 * <p>- The user is authenticated and has a role that has create set to true in
 * the security config of the destination.
 *</p>
 *
 * <p>Notes for JBossMQ developers:
 * All calls, except close(), that is possible to do on a connection must call
 * checkClientID()
 * </p>
 * 
 *
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version   $Revision: 1.20.2.7 $
 * @created   August 16, 2001
 */
public class Connection implements java.io.Serializable, javax.jms.Connection
{
   /**
    * Description of the Field
    */
   public static ThreadGroup threadGroup = new ThreadGroup("JBossMQ Client Threads");
   
   static Logger log = Logger.getLogger(Connection.class);
   
   /**
    * Maps a destination to a LinkedList of Subscriptions
    */
   public HashMap destinationSubscriptions = new HashMap();
   
   /**
    * Maps a a subsction id to a Subscription
    */
   public HashMap subscriptions = new HashMap();
   /**
    * Is the connection stopped ?
    */
   public boolean modeStop;
   //////////////////////////////////////////////////////////////
   // Attributes
   //////////////////////////////////////////////////////////////
   
   /**
    * This is our connection to the JMS server
    */
   protected ServerIL serverIL;
   
   /**
    * This is the clientID
    */
   protected String clientID;
   /**
    * The connection token is used to identify our connection to the server.
    */
   protected ConnectionToken connectionToken;
   
   /**
    * The object that sets up the client IL
    */
   protected ClientILService clientILService;
   
   /**
    * Manages the thread that pings the connection to see if it is 'alive'
    */
   static protected ClockDaemon clockDaemon = new ClockDaemon();
   
   /**
    * How often to ping the connection
    */
   protected long pingPeriod = 1000 * 60;
   
   /**
    * This feild is reset when a ping is sent, set when ponged.
    */
   protected boolean ponged=true;
   
   /**
    * This is used to know when the PingTask is running
    */
   Semaphore pingTaskSemaphore = new Semaphore(1);
   
   /**
    * Identifies the PinkTask in the ClockDaemon
    */
   Object pingTaskId;
   
   /**
    * Set a soon as close() is called on the connection.
    */
   protected volatile boolean closing = false;

   /**
    * 
    */
   private volatile boolean setClientIdAllowed = true;
   
   //LinkedList of all created sessions by this connection
   HashSet createdSessions;
   // Numbers subscriptions
   int subscriptionCounter = Integer.MIN_VALUE;
   Object subCountLock = new Object();
   //Is the connection closed ?
   boolean closed;
   // Used to control tranactions
   SpyXAResourceManager spyXAResourceManager;
   
   //The class that created this connection
   GenericConnectionFactory genericConnectionFactory;
   //Last message ID returned
   private int lastMessageID;
   
   //the exceptionListener
   private ExceptionListener exceptionListener;
   
   //Get a new messageID (creation of a new message)
   private StringBuffer sb = new StringBuffer();
   private char[] charStack = new char[22];

   String sessionId;
   
   /**
    * Static class initializer..
    */
   static
   {
      log.debug("Setting the clockDaemon's thread factory");
      clockDaemon.setThreadFactory(
         new ThreadFactory()
         {
            public Thread newThread(Runnable r)
            {
               Thread t = new Thread(threadGroup, r, "Connection Monitor Thread");
               t.setDaemon(true);
               return t;
            }
         }      
      );
   }

   //////////////////////////////////////////////////////////////
   // Constructors
   //////////////////////////////////////////////////////////////
   
   Connection(String userName, String password, GenericConnectionFactory genericConnectionFactory)
      throws JMSException
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("Connection Initializing");
      
      //Set the attributes
      createdSessions = new HashSet();
      connectionToken = null;
      closed = false;
      lastMessageID = 0;
      modeStop = true;
      
      // Connect to the server
      if( trace )
         log.trace("Getting the serverIL");
      this.genericConnectionFactory = genericConnectionFactory;
      serverIL = genericConnectionFactory.createServerIL();

      // Register ourselves as a client
      try
      {
         if( trace )
         {
            log.trace("serverIL="+serverIL);
            log.trace("Authenticating");
         }

         // Authenticate with the server
         authenticate(userName, password);

         if (userName != null)
         {
            askForAnID(userName, password);
         }

         // Setup the ClientIL service so that the Server can
         // push messages to us
         if( trace )
            log.trace("Starting the clientIL service");
         startILService();
      }
      catch (Exception e)
      {
         // Client registeration failed, close the connection
         try
         {
            serverIL.connectionClosing(null);
         }
         catch (Exception ex)
         {
            log.debug("Error closing the connection", ex);
         }

         if (e instanceof JMSException)
            throw (JMSException) e;
         else
            throw new SpyJMSException("Failed to create connection", e);
      }
      
      // Finish constructing the connection
      try
      {
         // Setup the XA Resource manager,
         spyXAResourceManager = new SpyXAResourceManager(this);
      
         // Used to monitor the connection.
         startPingThread();
         if( trace )
            log.trace("Connection establishment successful");
      }
      catch (Exception e)
      {
         // Could not complete the connection, tidy up
         // the server and client ILs.
         try
         {
            serverIL.connectionClosing(connectionToken);
         }
         catch (Exception ex)
         {
            log.debug("Error closing the connection", ex);
         }
         try
         {
            stopILService();
         }
         catch (Exception ex)
         {
            log.debug("Error stopping the client IL", ex);
         }

         if (e instanceof JMSException)
            throw (JMSException) e;
         else
            throw new SpyJMSException("Failed to create connection", e);
      }
   }

   //////////////////////////////////////////////////////////////
   // Constructors
   //////////////////////////////////////////////////////////////
   
   Connection(GenericConnectionFactory genericConnectionFactory)
   throws JMSException
   {
      this(null, null, genericConnectionFactory);
   }

   //////////////////////////////////////////////////////////////
   // Implementation of javax.jms.Connection
   //////////////////////////////////////////////////////////////
   

   /**
    * Sets the ClientID attribute of the Connection object.
    *
    * @param cID               The new ClientID value
    * @exception JMSException  Description of Exception
    * @see javax.jms.Connection
    */
   public void setClientID(String cID)
      throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      if (clientID != null)
      {
         throw new IllegalStateException("The connection has already a clientID");
      }
      if (setClientIdAllowed == false) {
         throw new IllegalStateException("SetClientID was not called emediately after creation of connection");
      }
      if( log.isTraceEnabled() )
         log.trace("SetClientID(" + clientID + ")");

      try
      {
         serverIL.checkID(cID);
      }
      catch (JMSException e)
      {
         throw e;
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot connect to the JMSServer", e);
      }
      
      clientID = cID;
      connectionToken.setClientID(clientID);

   }

   /**
    * Gets the ClientID attribute of the Connection object
    *
    * @return                  The ClientID value
    * @exception JMSException  Description of Exception
    * @see javax.jms.Connection
    */
   public String getClientID()
      throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      // checkClientID();
      return clientID;
   }
   
   /**
    * Gets the ExceptionListener attribute of the Connection object
    *
    * @return                  The ExceptionListener value
    * @exception JMSException  Description of Exception
    * @see javax.jms.Connection
    */
   public ExceptionListener getExceptionListener()
      throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      checkClientID();
      return exceptionListener;
   }
   /**
    * Sets the ExceptionListener attribute of the Connection object
    *
    * @param listener          The new ExceptionListener value
    * @exception JMSException  Description of Exception
    */
   public void setExceptionListener(ExceptionListener listener)
      throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      checkClientID();
      exceptionListener = listener;
   }

      
   /**
    * Gets the MetaData attribute of the Connection object
    *
    * @return                  The MetaData value
    * @exception JMSException  Description of Exception
    * @see javax.jms.Connection
    */
   public ConnectionMetaData getMetaData()
   throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      checkClientID();
      return new SpyConnectionMetaData();
   }

   /**
    * Close the connection.
    *
    * @exception JMSException  Description of Exception
    * @see javax.jms.Connection
    */
   
   public synchronized void close()
   throws JMSException
   {
      if (closed)
      {
         return;
      }
      // We do not check clientID on close(), not much worth...
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("Closing sessions, ClientID=" + connectionToken.getClientID());
      closing = true;

      // The first exception
      JMSException exception = null;

      //notify his sessions
      Object[] vect = null;
      synchronized (createdSessions)
      {
         vect = createdSessions.toArray();
      }
      for (int i = 0; i < vect.length; i++)
      {
         try
         {
            ((SpySession)vect[i]).close();
         }
         catch (JMSException e)
         {
            exception = e;
         }
         catch (Exception e)
         {
            exception = new SpyJMSException("Error closing sessions", e);
            exception.fillInStackTrace();
         }
      }

      if( trace )
      {
         log.trace("Closed sessions");
         log.debug("Notifiying the server of close");
      }

      //Notify the JMSServer that I am closing
      try
      {
         serverIL.connectionClosing(connectionToken);
      }
      catch (JMSException e)
      {
         if (exception == null)
            exception = e;
      }
      catch (Exception e)
      {
         if (exception == null)
         {
            exception = new SpyJMSException("Cannot close properly the connection", e);
            exception.fillInStackTrace();
         }
      }
      
      // Clean up after the ping thread..
      try
      {
         stopPingThread();
      }
      catch (Exception e)
      {
         if (exception == null)
         {
            exception = new SpyJMSException("Cannot stop the ping thread", e);
            exception.fillInStackTrace();
         }
      }
      
      if( trace )
         log.trace("Stoping the ClientIL service");
      try
      {
         stopILService();
      }
      catch (JMSException e)
      {
         if (exception == null)
            exception = e;
      }
      catch (Exception e)
      {
         if (exception == null)
         {
            exception = new SpyJMSException("Cannot stop the client il service", e);
            exception.fillInStackTrace();
         }
      }

      // Throw the first exception
      if (exception != null)
         throw exception;

      if( trace )
         log.trace("Disconnected from server");

      // Only set the closed flag after all the objects that depend
      // on this connection have been closed.
      closed = true;
   }
   
   /**
    * Start async message delivery.
    *
    * @exception JMSException  Description of Exception
    * @see javax.jms.Connection
    */
   public void start()
      throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      checkClientID();
      if (!modeStop)
      {
         return;
      }
      modeStop = false;

      if( log.isTraceEnabled() )
         log.trace("Starting connection, ClientID=" + connectionToken.getClientID());
      
      try
      {
         serverIL.setEnabled(connectionToken, true);
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot enable the connection with the JMS server", e);
      }
   }
   
   /**
    * Stop delivery of message.
    *
    * @exception JMSException  Description of Exception
    * @see javax.jms.Connection
    */
   public void stop()
      throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      checkClientID();
      if (modeStop)
      {
         return;
      }
      modeStop = true;
      if( log.isTraceEnabled() )
         log.trace("Stoping connection, ClientID=" + connectionToken.getClientID());
      
      try
      {
         serverIL.setEnabled(connectionToken, false);
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot disable the connection with the JMS server", e);
      }
   }

   //////////////////////////////////////////////////////////////
   // Public Methods
   //////////////////////////////////////////////////////////////
   


   
   /**
    * Gets the ServerIL attribute of the Connection object
    *
    * @return   The ServerIL value
    */
   public ServerIL getServerIL()
   {
      return serverIL;
   }
   
   /**
    * #Description of the Method
    */
   public void asynchClose()
   {
   }
   
   //called by a TemporaryDestination which is going to be deleted()
   /**
    * #Description of the Method
    *
    * @param dest  Description of Parameter
    */
   public void asynchDeleteTemporaryDestination(SpyDestination dest)
   {
      try
      {
         deleteTemporaryDestination(dest);
      }
      catch (JMSException e)
      {
         asynchFailure(e.getMessage(), e.getLinkedException());
      }
      
   }
   
   //Gets the first consumer that is listening to a destination.
   /**
    * #Description of the Method
    *
    * @param requests  Description of Parameter
    */
   public void asynchDeliver(ReceiveRequest requests[])
   {
      // If we are closing the connection, the server will nack the messages
      if (closing)
         return;
      
      try
      {
         for (int i = 0; i < requests.length; i++)
         {
            
            SpyConsumer consumer = (SpyConsumer)subscriptions.get(requests[i].subscriptionId);
            requests[i].message.createAcknowledgementRequest(requests[i].subscriptionId.intValue());
            
            if (consumer == null)
            {
               send(requests[i].message.getAcknowledgementRequest(false));
               log.debug("WARNING: NACK issued due to non existent subscription");
               continue;
            }
            
            consumer.addMessage(requests[i].message);
         }
      }
      catch (JMSException e)
      {
         asynchFailure(e.getMessage(), e.getLinkedException());
      }
   }
   
   /**
    * #Description of the Method
    *
    * @param reason  Description of Parameter
    * @param e       Description of Parameter
    */
   public void asynchFailure(String reason, Exception e)
   {
      
      // Exceptions due to closing will be ignored.
      if (closing)
      {
         return;
      }
      
      JMSException excep;
      if (e instanceof JMSException)
         excep = (JMSException) e;
      else
      {
         excep = new SpyJMSException(reason, e);
         excep.fillInStackTrace();
      }
      
      if (exceptionListener != null)
      {
         synchronized (exceptionListener)
         {
            if (exceptionListener != null)
               exceptionListener.onException(excep);
            else
              log.warn("Connection failure: ", excep);
         }
      }
      else
         log.warn("Connection failure: ", excep);
   }
   
   /**
    * #Description of the Method
    *
    * @param serverTime  Description of Parameter
    */
   public void asynchPong(long serverTime)
   {
      if( log.isTraceEnabled() )
         log.trace("PONG, serverIL="+serverIL);
      ponged = true;
   }


   
   //called by a TemporaryDestination which is going to be deleted()
   /**
    * #Description of the Method
    *
    * @param dest              Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void deleteTemporaryDestination(SpyDestination dest)
      throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }

      if (log.isDebugEnabled())
         log.debug("SpyConnection: deleteDestination(dest=" + dest.toString() + ")");
      try
      {
         
         //Remove it from the destinations list
         synchronized (subscriptions)
         {
            destinationSubscriptions.remove(dest);
         }
         
         //Notify its sessions that this TemporaryDestination is going to be deleted()
         //We could do that only on the Sessions "linked" to this Destination
         synchronized (createdSessions)
         {
            
            Iterator i = createdSessions.iterator();
            while (i.hasNext())
            {
               ((SpySession)i.next()).deleteTemporaryDestination(dest);
            }
            
         }
         
         //Ask the broker to delete() this TemporaryDestination
         serverIL.deleteTemporaryDestination(connectionToken, dest);
         
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot delete the TemporaryDestination", e);
      }
      
   }
   
   /**
    * Check that a clientID exists. If not get one from server.
    *
    * Also sets the setClientIdAllowed to false.
    *
    * Check clientId, must be called by all public methods on the
    * jacax.jmx.Connection interface and its children.
    * @exception JMSException  if clientID is null as post condition
    */
   synchronized 
   protected void  checkClientID() 
      throws JMSException{
      if (setClientIdAllowed == false)
         return;//No need to do anything
      
      boolean trace = log.isTraceEnabled();
      setClientIdAllowed = false;
      if (trace)
         log.trace("Checking clientID :" + clientID);
      if (clientID == null) {
         askForAnID();//Request a random one
         if (clientID == null)
            throw new JMSException("Could not get a clientID");
         connectionToken.setClientID(clientID);
         
         if (trace)
            log.trace("Connection establishment successful");
      }
   }
   
   //ask the JMS server for a new ID
   /**
    * #Description of the Method
    *
    * @exception JMSException  Description of Exception
    */
   protected void askForAnID()
   throws JMSException
   {
      try
      {
         clientID = serverIL.getID();
      }
      catch (Exception e)
      {
         log.debug("Cannot get a client ID:", e);
         throw new SpyJMSException("Cannot get a client ID: " + e.getMessage(), e);
      }
   }
   
   //ask the JMS server for a new ID
   /**
    * #Description of the Method
    *
    * @param userName          Description of Parameter
    * @param password          Description of Parameter
    * @exception JMSException  Description of Exception
    */
   protected void askForAnID(String userName, String password)
   throws JMSException
   {
      try
      {
         clientID = serverIL.checkUser(userName, password);
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot get a client ID", e);
      }
   }

      protected void authenticate(String userName, String password)
   throws JMSException
   {
      try
      {
         log.trace("Authenticating user " + userName);
         sessionId = serverIL.authenticate(userName, password);
      }
      catch(JMSException ex) {
         throw ex;
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot authenticate user", e);
      }
   }
   
   // used to acknowledge a message
   /**
    * #Description of the Method
    *
    * @param item              Description of Parameter
    * @exception JMSException  Description of Exception
    */
   protected void send(AcknowledgementRequest item)
   throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      try
      {
         serverIL.acknowledge(connectionToken, item);
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot acknowlege a message", e);
      }
   }
   
   // Used to commit/rollback a transaction.
   /**
    * #Description of the Method
    *
    * @param transaction       Description of Parameter
    * @exception JMSException  Description of Exception
    */
   protected void send(TransactionRequest transaction)
   throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      
      try
      {
         serverIL.transact(connectionToken, transaction);
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot process a transaction", e);
      }
      
   }
   
   ////////////////////////////////////////////////////////////////////
   // Protected
   ////////////////////////////////////////////////////////////////////
   
   //create a new Distributed object which receives the messages for this connection
   /**
    * #Description of the Method
    *
    * @exception JMSException  Description of Exception
    */
   protected void startILService()
   throws JMSException
   {
      try
      {
         
         clientILService = genericConnectionFactory.createClientILService(this);
         clientILService.start();
         connectionToken = new ConnectionToken(clientID, clientILService.getClientIL(), sessionId);
         serverIL.setConnectionToken(connectionToken);
         
      }
      catch (Exception e)
      {
         log.debug("Cannot start a the client IL service", e);
         throw new SpyJMSException("Cannot start a the client IL service", e);
      }
   }
   
   ////////////////////////////////////////////////////////////////////
   // Protected
   ////////////////////////////////////////////////////////////////////
   
   //create a new Distributed object which receives the messages for this connection
   /**
    * #Description of the Method
    *
    * @exception JMSException  Description of Exception
    */
   protected void stopILService()
   throws JMSException
   {
      try
      {
         
         clientILService.stop();
         
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot stop a the client IL service", e);
      }
   }
   
   //all longs are less than 22 digits long
   
   //Note that in this routine we assume that System.currentTimeMillis() is non-negative
   //always be non-negative (so don't set lastMessageID to a positive for a start).
   String getNewMessageID()
   throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      
      synchronized (sb)
      {
         sb.setLength(0);
         sb.append(clientID);
         sb.append('-');
         long time = System.currentTimeMillis();
         int count = 0;
         do
         {
            charStack[count] = (char)('0' + (time % 10));
            time = time / 10;
            ++count;
         } while (time != 0);
         --count;
         for (; count >= 0; --count)
         {
            sb.append(charStack[count]);
         }
         ++lastMessageID;
         //avoid having to deal with negative numbers.
         if (lastMessageID < 0)
         {
            lastMessageID = 0;
         }
         int id = lastMessageID;
         count = 0;
         do
         {
            charStack[count] = (char)('0' + (id % 10));
            id = id / 10;
            ++count;
         } while (id != 0);
         --count;
         for (; count >= 0; --count)
         {
            sb.append(charStack[count]);
         }
         return sb.toString();
      }
   }
   
   //A new Consumer has been created for the Destination dest
   // We have to handle security issues, a consumer may actually not be allowed
   // to be created
   void addConsumer(SpyConsumer consumer)
      throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      
      Subscription req = consumer.getSubscription();
      synchronized(subCountLock)
      {
         req.subscriptionId = subscriptionCounter++;
      }
      req.connectionToken = connectionToken;
      if( log.isTraceEnabled() )
         log.trace("Connection: addConsumer(dest=" + req.destination.toString() + ")");
      
      try
      {
         
         synchronized (subscriptions)
         {
            
            subscriptions.put(new Integer(req.subscriptionId), consumer);
            
            LinkedList ll = (LinkedList)destinationSubscriptions.get(req.destination);
            if (ll == null)
            {
               ll = new LinkedList();
               destinationSubscriptions.put(req.destination, ll);
            }
            
            ll.add(consumer);
         }
         
         serverIL.subscribe(connectionToken, req);
         
      }
      catch(JMSSecurityException ex) {
         removeConsumerInternal(consumer);
         throw ex;
      }
      catch(JMSException ex) {
         throw ex;
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot subscribe to this Destination: " + e.getMessage(), e);
      }
      
   }

   
   /**
    * @param queue             Description of Parameter
    * @param selector          Description of Parameter
    * @return                  org.jboss.mq.distributed.interfaces.ServerIL
    * @exception JMSException  Description of Exception
    */
   SpyMessage[] browse(Queue queue, String selector)
   throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      
      try
      {
         return serverIL.browse(connectionToken, queue, selector);
      }
      catch(JMSException ex) {
         throw ex;
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot browse the Queue", e);
      }
   }
   
   //Send a message to the serverIL
   void pingServer(long clientTime)
      throws JMSException
   {
      
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      
      try
      {
         if( log.isTraceEnabled() )
            log.trace("PING");
         serverIL.ping(connectionToken, clientTime);
         
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot ping the JMS server", e);
      }
   }
   
   /**
    * @param sub               Description of Parameter
    * @param wait              Description of Parameter
    * @return                  org.jboss.mq.distributed.interfaces.ServerIL
    * @exception JMSException  Description of Exception
    */
   SpyMessage receive(Subscription sub, long wait)
   throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      
      try
      {
         SpyMessage message = serverIL.receive(connectionToken, sub.subscriptionId, wait);
         if (message != null)
         {
            message.createAcknowledgementRequest(sub.subscriptionId);
         }
         return message;
      }
      catch(JMSException ex) {
         throw ex;
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot create a ConnectionReceiver", e);
      }
   }
   
   //A consumer does not need to receieve the messages from a Destination
   void removeConsumer(SpyConsumer consumer)
   throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      
      Subscription req = consumer.getSubscription();
      if( log.isDebugEnabled() )
         log.debug("Connection: removeSession(dest=" + req.destination + ")");

      try
      {
         
         serverIL.unsubscribe(connectionToken, req.subscriptionId);

         removeConsumerInternal(consumer);
      }
      catch (JMSException e)
      {
         throw e;
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot unsubscribe to this destination", e);
      }
      
   }

   private void removeConsumerInternal(SpyConsumer consumer) {
      synchronized (subscriptions)
      {
         Subscription req = consumer.getSubscription();
         subscriptions.remove(new Integer(req.subscriptionId));
         
         LinkedList ll = (LinkedList)destinationSubscriptions.get(req.destination);
         if (ll != null)
         {
            ll.remove(consumer);
            if (ll.size() == 0)
            {
               destinationSubscriptions.remove(req.destination);
            }
         }
      }
   }
   
   //Send a message to the serverIL
   void sendToServer(SpyMessage mes)
      throws JMSException
   {
      if (closed)
      {
         throw new IllegalStateException("The connection is closed");
      }
      
      try
      {
         
         serverIL.addMessage(connectionToken, mes);
         
      }
      catch(JMSException ex) {
         throw ex;
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot send a message to the JMS server", e);
      }
   }
   
   //Called by a session when it is closing
   void sessionClosing(SpySession who)
   {
      synchronized (createdSessions)
      {
         createdSessions.remove(who);
      }
      
      //This session should not be in the "destinations" object anymore.
      //We could check this, though
   }
   
   void unsubscribe(DurableSubscriptionID id)
   throws JMSException
   {
      try
      {
         serverIL.destroySubscription(connectionToken,id);
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Cannot destroy durable subscription " + id, e);
      }
   }
   
   /**
    * #Description of the Class
    */
   class PingTask implements Runnable
   {
      /**
       * Main processing method for the PingTask object
       */
      public void run()
      {
         try
         {
            pingTaskSemaphore.acquire();
         } catch ( InterruptedException e )
         {
            return;
         }
         try
         {
            if (ponged == false)
            {
               // Server did not pong use with in the timeout
               // period..  Assuming the connection is dead.
               throw new SpyJMSException("", new IOException("ping timeout."));
            }
            
            ponged = false;
            pingServer(System.currentTimeMillis());
         }
         catch (JMSException e)
         {
            asynchFailure("Connection Failed", e.getLinkedException());
         } finally
         {
            pingTaskSemaphore.release();}
      }
   }
   
   private void startPingThread()
   {
      
      // Ping thread does not need to be running if the
      // ping period is 0.
      if( pingPeriod == 0 )
         return;
      pingTaskId = clockDaemon.executePeriodically(pingPeriod,new PingTask(), true);
      
   }
   
   private void stopPingThread()
   {
      
      // Ping thread was not running if ping period is 0.
      if( pingPeriod == 0 )
         return;
      
      clockDaemon.cancel(pingTaskId);
      
      //Aquire the Semaphore to make sure the ping task is not running.
      try
      {
         pingTaskSemaphore.attempt(1000 * 10);
      }
      catch (InterruptedException e)
      {
         Thread.currentThread().interrupt();
      }
   }
}
