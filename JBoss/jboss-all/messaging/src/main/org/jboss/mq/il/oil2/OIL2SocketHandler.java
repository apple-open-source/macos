/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil2;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.HashMap;
import java.util.Iterator;

import org.jboss.logging.Logger;

import EDU.oswego.cs.dl.util.concurrent.BoundedBuffer;
import EDU.oswego.cs.dl.util.concurrent.Channel;
import EDU.oswego.cs.dl.util.concurrent.ConcurrentHashMap;
import EDU.oswego.cs.dl.util.concurrent.LinkedQueue;
import EDU.oswego.cs.dl.util.concurrent.PooledExecutor;
import EDU.oswego.cs.dl.util.concurrent.Slot;
import EDU.oswego.cs.dl.util.concurrent.ThreadFactory;

/**
 * The OIL2 implementation of the ServerIL object
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version   $Revision: 1$
 */
public final class OIL2SocketHandler implements java.lang.Cloneable, Runnable
{
   final static private Logger log = Logger.getLogger(OIL2SocketHandler.class);

   /**
    * Messages will be read from the input stream
    */
   private ObjectInputStream in;

   /**
    * Messages will be writen to the output stream
    */
   private ObjectOutputStream out;

   /** 
    * Should we be receiving messages??
    */
   private boolean running;

   /** 
    * The thread group that the reader thread should join.
    */
   private final ThreadGroup partentThreadGroup;

   /** 
    * Reader thread.
    */
   private Thread worker;

   /**
    * Number of OIL2 Worker threads started.
    */
   private static int threadNumber = 0;

   /**
    * The state of the handler
    */
   private static final int STATE_CREATED = 0;
   private static final int STATE_CONNECTED = 1;
   private static final int STATE_DISCONNECTED = 2;
   private static final int STATE_CONNECTION_ERROR = 3;
   private int state = STATE_CREATED;

   /**
    * Requst create slots to wait for responses,
    * those slots are stored in this hashmap.
    * 
    * This field uses copy on write semantics.
    */
   volatile ConcurrentHashMap responseSlots = new ConcurrentHashMap();

   /**
    * The request listner is notified of new requests
    * and of asyncronous IO errors.
    */
   OIL2RequestListner requestListner;

   /**
    * If the socket handler is currently pumping messages.
    */
   private volatile boolean pumpingData = false;

   /**
    * Pump mutex
    */
   private Object pumpMutex = new Object();

   /**
    * The that new request get placed into when they arrived.
    */
   LinkedQueue requestQueue = new LinkedQueue();

   /**
    * The thread pool used to service incoming requests..
    */
   PooledExecutor pool;

   /**
    * Constructor for the OILServerIL object
    *
    * @param a     Description of Parameter
    * @param port  Description of Parameter
    */
   public OIL2SocketHandler(ObjectInputStream in, ObjectOutputStream out, ThreadGroup partentThreadGroup)
   {
      this.in = in;
      this.out = out;
      this.partentThreadGroup = partentThreadGroup;

      synchronized (OIL2SocketHandler.class)
      {
         if (pool == null)
         {
            pool = new PooledExecutor(50);
            // supply a ThreadFactory to the pool to create daemon threads
            log.debug("Setting the OIL2SocketHandler's thread factory");
            pool.setThreadFactory(
               new ThreadFactory()
               {
                  private int threadNo = 0;
                  public Thread newThread(Runnable r)
                  {
                     Thread t = new Thread(OIL2SocketHandler.this.partentThreadGroup, r, "OIL2SocketHandler Thread-" + threadNo++);
                     t.setDaemon(true);
                     return t;
                  }
               }      
            );
            pool.setMinimumPoolSize(1);
            pool.setKeepAliveTime(1000 * 60);
            pool.runWhenBlocked();
            pool.createThreads(1);
         }
      }
   }

   /**
    * #Description of the Method
    *
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   public void sendRequest(OIL2Request request) throws IOException
   {
//      if (log.isTraceEnabled())
//         log.trace("Sending request: " + request);

      try
      {
         synchronized (out)
         {
            out.writeByte(1);
            request.writeExternal(out);
            out.reset();
            out.flush();
         }
      }
      catch (IOException e)
      {
         state = STATE_CONNECTION_ERROR;
         throw e;
      }

   }

   /**
    * #Description of the Method
    */
   private void registerResponseSlot(OIL2Request request, Slot responseSlot) throws IOException
   {
      responseSlots.put(request.requestId, responseSlot);
   }

   /**
    * #Description of the Method
    */
   public void setRequestListner(OIL2RequestListner requestListner)
   {
      this.requestListner = requestListner;
   }

   /**
    * #Description of the Method
    *
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   public void sendResponse(OIL2Response response) throws IOException
   {
//      if (log.isTraceEnabled())
//         log.trace("Sending response: " + response);

      try
      {
         synchronized (out)
         {
            out.writeByte(2);
            response.writeExternal(out);
            out.reset();
            out.flush();
         }
      }
      catch (IOException e)
      {
         state = STATE_CONNECTION_ERROR;
         throw e;
      }
   }

   /**
    *  Pumps messages from the input stream.
    *  
    *  If the request object is not null, then the target message is 
    *  the response object for the request argument.  The target
    *  message is returned.
    * 
    *  If the request object is null, then the target message is 
    *  the first new request that is encountered.  The new request 
    *  messag is returned.
    * 
    *  All message received before the target message are pumped.
    *  A pumped message is placed in either Response Slots or
    *  the Request Queue depending on if the message is a response
    *  or requests.
    * 
    * @param request The request object that is waiting for a response.
    * @return the request or reponse object that this method was looking for
    * @exception  IOException  Description of Exception
    */
   private Object pumpMessages(OIL2Request request, Channel mySlot)
      throws IOException, ClassNotFoundException, InterruptedException
   {

      synchronized (pumpMutex)
      {
         // Is somebody else pumping data??
         if (pumpingData)
         {
            return null;
         }
         else
            pumpingData = true;
      }

      try
      {
         while (true)
         {
            if (mySlot != null)
            {
               // Do we have our response sitting in our slot allready??
               Object o;
               while ((o = mySlot.peek()) != null)
               {
                  o = mySlot.take();
                  if (o != this)
                  {
                     return o;
                  }
               }
            }

//            if (log.isTraceEnabled())
//               log.trace("Waiting for message");
            byte code = in.readByte();
            boolean tracing = log.isTraceEnabled();
            switch (code)
            {
               // Request received... pass it up
               case 1 :
//                  if (tracing)
//                     log.trace("Reading Request Message");
                  OIL2Request newRequest = new OIL2Request();
                  newRequest.readExternal(in);

                  // Are we looking for a request??
                  if (request == null)
                  {
//                     if (tracing)
//                        log.trace("Target message arrvied: returning the new request: "+newRequest);
                     return newRequest;
                  }
                  else
                  {
//                     if (tracing)
//                        log.trace("Not the target message: queueing the new request: "+newRequest);
                     requestQueue.put(newRequest);
                  }

                  break;

                  // Response received... find the response slot
               case 2 :
//                  if (tracing)
//                     log.trace("Reading Response Message");

                  OIL2Response response = new OIL2Response();
                  response.readExternal(in);

                  // No reponse id to response to..
                  if (response.correlationRequestId == null)
                     continue;

                  // Is this the response object we are looking for
                  if (request != null && request.requestId.equals(response.correlationRequestId))
                  {
//                     if (log.isTraceEnabled())
//                        log.trace("Target message arrvied: returning the response: "+response);
                     return response;
                  }
                  else
                  {
//                     if (log.isTraceEnabled())
//                        log.trace("Not the target message: Sending to request slot: "+response);

                     Slot slot = (Slot) responseSlots.remove(response.correlationRequestId);

                     if (slot != null)
                     {
                        slot.put(response);
                     }
                     else
                     {
                        // This should not happen...
                        if (log.isTraceEnabled())
                           log.warn("No slot registered for: " + response);
                     }
                  }
                  break;
            } // switch
         } // while         
      }
      finally
      {
         synchronized (pumpMutex)
         {
            pumpingData = false;
         }

         Thread thread = Thread.currentThread();
         boolean interrupted = thread.isInterrupted();

         // We are done, let somebody know that they can 
         // start pumping us again.
         Iterator i = responseSlots.values().iterator();
         while (i.hasNext())
         {
            Slot s = (Slot) i.next();
            if (s != mySlot)
               s.offer(this, 0);
         }

         // Only notify the request waiter if we are not
         // giving him a message on this method call.
         if (request != null)
         {
            requestQueue.put(this);
         }

         if (interrupted)
            thread.interrupt();
      }
   }

   public OIL2Response synchRequest(OIL2Request request)
      throws IOException, InterruptedException, ClassNotFoundException
   {

      //      if (log.isTraceEnabled())
      //         log.trace("Sending request: "+request);

      Slot slot = new Slot();
      registerResponseSlot(request, slot);
      sendRequest(request);

      Object o = null;
      while (true)
      {
         // Do we have something in our queue??
         if (o != null)
         {
            // was is a request message??
            if (o != this)
            {
               //               if (log.isTraceEnabled())
               //                  log.trace("Got response: "+o);
               return (OIL2Response) o;
            }
            // See if we have another message in the queue.
            o = slot.peek();
            if (o != null)
               o = slot.take();
         }
         else
         {
            // We did not have any messages in the slot,
            // so we have to go pumping..
            o = pumpMessages(request, slot);
            if (o == null)
            {
               // Somebody else is in the pump, wait till we 
               // are notified to get in.
               o = slot.take();
            }
         }
      } // end while
   }

   public class RequestRunner implements Runnable
   {
      OIL2Request request;
      RequestRunner(OIL2Request request)
      {
         this.request = request;
      }
      public void run()
      {
         requestListner.handleRequest(request);
      }
   }

   /**
    * Main processing method for the OILClientILService object
    */
   public void run()
   {
      int code = 0;
      OIL2Request request;

      try
      {

         Object o = null;
         while (running)
         {
            // Do we have something in our queue??
            if (o != null)
            {
               // was is a request message??
               if (o != this)
               {
                  pool.execute(new RequestRunner((OIL2Request) o));
               }
               // See if we have another message in the queue.
               o = requestQueue.peek();
               if (o != null)
                  o = requestQueue.take();
            }
            else
            {
               // We did not have any messages in the queue,
               // so we have to go pumping..
               o = pumpMessages(null, requestQueue);
               if (o == null)
               {
                  // Somebody else is in the pump, wait till we 
                  // are notified to get in.
                  o = requestQueue.take();
               }
            }
         } // end while

      }
      catch (InterruptedException e)
      {
         if (log.isTraceEnabled())
            log.trace("Stopped due to interruption");
      }
      catch (Exception e)
      {
         if (log.isTraceEnabled())
            log.trace("Stopping due to unexcpected exception: ", e);
         state = STATE_CONNECTION_ERROR;
         requestListner.handleConnectionException(e);
      }

      // ensure the flag is set correctly
      running = false;
      if (log.isTraceEnabled())
         log.trace("Stopped");
   }

   public void start() //throws java.lang.Exception
   {
      if (log.isTraceEnabled())
         log.trace("Starting");

      running = true;
      worker = new Thread(partentThreadGroup, this, "OIL2 Worker-" + threadNumber++);
      worker.setDaemon(true);
      worker.start();

   }

   public void stop()
   {
      if (log.isTraceEnabled())
         log.trace("Stopping");
      running = false;
      worker.interrupt();
   }

}
