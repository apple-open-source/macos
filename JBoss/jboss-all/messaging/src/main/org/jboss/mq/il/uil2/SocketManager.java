/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.Socket;
import java.util.Iterator;

import EDU.oswego.cs.dl.util.concurrent.ConcurrentHashMap;
import EDU.oswego.cs.dl.util.concurrent.LinkedQueue;
import EDU.oswego.cs.dl.util.concurrent.PooledExecutor;

import org.jboss.mq.il.uil2.msgs.BaseMsg;
import org.jboss.mq.SpyJMSException;
import org.jboss.util.stream.NotifyingBufferedInputStream;
import org.jboss.util.stream.NotifyingBufferedOutputStream;
import org.jboss.logging.Logger;

/** Used to manage the client/server and server/client communication in an
 * asynchrounous manner.
 *
 * @todo verify the pooled executor config
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.7 $
 */
public class SocketManager
{
   private static Logger log = Logger.getLogger(SocketManager.class);

   private static final int STOPPED = 0;
   private static final int STARTED = 1;
   private static final int STOPPING = 2;
   private static int taskID = 0;

   /** The socket created by the IL layer */
   private Socket socket;
   /** The input stream used by the read task */
   private ObjectInputStream in;
   /** The buffering for output */
   NotifyingBufferedInputStream bufferedInput;
   /** The output stream used by the write task */
   private ObjectOutputStream out;
   /** The buffering for output */
   NotifyingBufferedOutputStream bufferedOutput;
   /** The write task thread */
   private Thread writeThread;
   /** The read task thread */
   private Thread readThread;
   /** The thread pool used to service incoming requests */
   PooledExecutor pool;
   /** The flag used to control the read loop */
   private int readState = STOPPED;
   /** The flag used to control the write loop */
   private int writeState = STOPPED;
   /** Used for constrolling the state */
   private Boolean running = new Boolean(false);
   /** The queue of messages to be processed by the write task */
   private LinkedQueue sendQueue;
   /** A HashMap<Integer, BaseMsg> that are awaiting a reply */
   private ConcurrentHashMap replyMap;
   /** The callback handler used for msgs that are not replys */
   private SocketManagerHandler handler;
   /** The buffer size */
   private int bufferSize = 1;
   /** The chunk size for notification of stream activity */
   private int chunkSize = 0x40000000;
   /** The logging trace level which is set in the ctor */
   private boolean trace;

   public SocketManager(Socket s)
      throws IOException
   {
      socket = s;
      sendQueue = new LinkedQueue();
      replyMap = new ConcurrentHashMap();
      trace = log.isTraceEnabled();
   }

   /** Start the read and write threads using the given thread group and
    * names of "UIL2.SocketManager.ReadTask" and "UIL2.SocketManager.WriteTask".
    * @param tg the thread group to use for the read and write threads.
    */
   public void start(ThreadGroup tg)
   {
      if( trace )
         log.trace("start called", new Exception("Start stack trace"));

      if (pool == null)
      {
         // TODO: Check the validity of this config
         pool = new PooledExecutor(5);
         pool.setMinimumPoolSize(1);
         pool.setKeepAliveTime(1000 * 60);
         pool.runWhenBlocked();
      }

      synchronized(running)
      {
         readState = STARTED;
         writeState = STARTED;
         running = new Boolean(true);
      }
      taskID ++;
      ReadTask readTask = new ReadTask();
      readThread = new Thread(tg, readTask, "UIL2.SocketManager.ReadTask#"+taskID);
      readThread.setDaemon(true);
      readThread.start();
      taskID ++;
      WriteTask writeTask = new WriteTask();
      writeThread = new Thread(tg, writeTask, "UIL2.SocketManager.WriteTask#"+taskID);
      writeThread.setDaemon(true);
      writeThread.start();
   }

   /** Stop the read and write threads by interrupting them.
    */
   public void stop()
   {
      synchronized (running)
      {
         if (readState == STARTED)
         {
            readState = STOPPING;
            readThread.interrupt();
         }
         if (writeState == STARTED)
         {
            writeState = STOPPING;
            writeThread.interrupt();
         }
         running = new Boolean(false);
      }
   }

   /** Set the callback handler for msgs that were not originated by the
    * socket manager. This is any msgs read that was not sent via the
    * sendMessage method.
    *
    * @param handler
    */
   public void setHandler(SocketManagerHandler handler)
   {
      this.handler = handler;
      if (bufferedInput != null)
         bufferedInput.setStreamListener(handler);
      if (bufferedOutput != null)
         bufferedOutput.setStreamListener(handler);
   }

   /**
    * Sets the buffer size
    *
    * @param size the size of the buffer
    */
   public void setBufferSize(int size)
   {
      this.bufferSize = size;
   }

   /**
    * Sets the chunk size
    *
    * @param size the size of a chunk
    */
   public void setChunkSize(int size)
   {
      this.chunkSize = size;
   }

   /** Send a two-way message and block the calling thread until the
    * msg reply is received. This enques the msg to the sendQueue, places
    * the msg in the replyMap and waits on the msg. The msg is notified by the
    * read task thread when it finds a msg with a msgID that maps to the
    * msg in the msgReply map.
    *
    * @param msg the request msg to send
    * @throws Exception thrown if the reply message has an error value
    */
   public void sendMessage(BaseMsg msg)
      throws Exception
   {
      internalSendMessage(msg, true);
      if( msg.error != null )
      {
         if( trace )
            log.trace("sendMessage will throw error", msg.error);
         throw msg.error;
      }
   }
   /** Send a one-way message. Typically this is a reply message, but it
    * could be an asynchronous msg that does not need a reply as it true
    * for ping msgs.
    *
    * @param msg
    * @throws Exception
    */
   public void sendReply(BaseMsg msg)
      throws Exception
   {
      msg.trimReply();
      internalSendMessage(msg, false);
   }

   /** This places the msg into the sendQueue and returns if waitOnReply
    * is false, or enques the msg to the sendQueue, places the msg
    * in the replyMap and waits on the msg.
    *
    * @param msg
    * @param waitOnReply
    * @throws Exception
    */
   private void internalSendMessage(BaseMsg msg, boolean waitOnReply)
      throws Exception
   {
      synchronized(running)
      {
         if (running.booleanValue() == false)
            throw new IOException("Client is not connected");
      }

      if( waitOnReply )
      {  // Send a request msg and wait for the reply
         synchronized( msg )
         {
            // Create the request msgID
            int msgID = msg.getMsgID();
            if( trace )
               log.trace("Begin internalSendMessage, round-trip msg="+msg);
            // Place the msg into the write queue and reply map
            replyMap.put(msg, msg);
            sendQueue.put(msg);
            // Wait for the msg reply
            msg.wait();
         }
      }
      else
      {  // Send an asynchronous msg, typically a reply
         if( trace )
            log.trace("Begin internalSendMessage, one-way msg="+msg);
         sendQueue.put(msg);
      }
      if( trace )
         log.trace("End internalSendMessage, msg="+msg);
   }

   /** The task managing the socket read thread
    *
    */
   public class ReadTask implements Runnable
   {
      public void run()
      {
         int msgType = 0;
         log.debug("Begin ReadTask.run");
         try
         {
            bufferedInput = new NotifyingBufferedInputStream(socket.getInputStream(), bufferSize, chunkSize, handler);
            in = new ObjectInputStream(bufferedInput);
            log.debug("Created ObjectInputStream");
         }
         catch(IOException e)
         {
            log.error("Failed to create ObjectInputStream", e);
            return;
         }

         while (true)
         {
            try
            {
               msgType = in.readByte();
               int msgID = in.readInt();
               if( trace )
                  log.trace("Read msgType: "+BaseMsg.toString(msgType)+", msgID: "+msgID);
               // See if there is a msg awaiting a reply
               BaseMsg key = new BaseMsg(msgType, msgID);
               BaseMsg msg = (BaseMsg) replyMap.remove(key);
               if( msg == null )
               {
                  msg = BaseMsg.createMsg(msgType);
                  msg.setMsgID(msgID);
                  msg.read(in);
                  if( trace )
                     log.trace("Read new msg: "+msg);

                  // Handle the message
                  msg.setHandler(this);
                  pool.execute(msg);
               }
               else
               {
                  if( trace )
                     log.trace("Found replyMap msg: "+msg);
                  msg.setMsgID(msgID);
                  try
                  {
                     msg.read(in);
                     if( trace )
                        log.trace("Read msg reply: "+msg);
                  }
                  catch (Throwable e)
                  {
                     // Forward the error to the waiting message
                     msg.setError(e);
                     throw e;
                  }
                  // Always notify the waiting message
                  finally
                  {
                     synchronized( msg )
                     {
                        msg.notify();
                     }
                  }
               }
            }
            catch(ClassNotFoundException e)
            {
               handleStop("Failed to read msgType:"+msgType, e);
               break;
            }
            catch(IOException e)
            {
               handleStop("Exiting on IOE", e);
               break;
            }
            catch(InterruptedException e)
            {
               handleStop("Exiting on interrupt", e);
               break;
            }
            catch(Throwable e)
            {
               handleStop("Exiting on unexpected error in read task", e);
               break;
            }
         }
         log.debug("End ReadTask.run");
      }

      /**
       * Handle the message or respond with an error
       */
      public void handleMsg(BaseMsg msg)
      {
         try
         {
            handler.handleMsg(msg);
         }
         catch(Throwable e)
         {
            log.error("Failed to handle: " + msg.toString(), e);
            msg.setError(e);
            try
            {
               internalSendMessage(msg, false);
            }
            catch(Exception ie)
            {
               log.warn("Failed to send error reply", ie);
            }
         }
      }

      /**
       * Stop the read thread
       */
      private void handleStop(String error, Throwable e)
      {
         synchronized(running)
         {
            readState = STOPPING;
            running = new Boolean(false);
         }

         if (e instanceof IOException || e instanceof InterruptedException)
         {
            if (trace)
               log.trace(error, e);
         }
         else
            log.error(error, e);

         replyAll(e);
         if (handler != null)
         {
            handler.asynchFailure(error, e);
            handler.close();
         }

         synchronized(running)
         {
            readState = STOPPED;
            if (writeState == STARTED)
            {
               writeState = STOPPING;
               writeThread.interrupt();
            }
         }

         try
         {
            in.close();
         }
         catch(Exception ignored)
         {
            if (trace)
               log.trace(ignored.getMessage(), ignored);
         }

         try
         {
            socket.close();
         }
         catch(Exception ignored)
         {
            if (trace)
               log.trace(ignored.getMessage(), ignored);
         }
      }

      private void replyAll(Throwable e)
      {
         // Clear the interrupted state of the thread
         Thread.currentThread().interrupted();

         for (Iterator iterator = replyMap.keySet().iterator(); iterator.hasNext();)
         {
            BaseMsg msg = (BaseMsg) iterator.next();
            msg.setError(e);
            synchronized(msg)
            {
               msg.notify();
            }
            iterator.remove();
         }
      }
   }

   /** The task managing the socket write thread
    *
    */
   public class WriteTask implements Runnable
   {
      public void run()
      {
         int msgType = 0;
         log.debug("Begin WriteTask.run");
         try
         {
            bufferedOutput = new NotifyingBufferedOutputStream(socket.getOutputStream(), bufferSize, chunkSize, handler);
            out = new ObjectOutputStream(bufferedOutput);
            log.debug("Created ObjectOutputStream");
         }
         catch(IOException e)
         {
            log.error("Failed to create ObjectOutputStream", e);
            return;
         }

         while (true)
         {
            BaseMsg msg = null;
            try
            {
               msg = (BaseMsg) sendQueue.take();
               if( trace )
                  log.trace("Write msg: "+msg);
               msg.write(out);
               out.reset();
               out.flush();
            }
            catch(InterruptedException e)
            {
               handleStop(msg, "WriteTask was interrupted", e);
               break;
            }
            catch(IOException e)
            {
               handleStop(msg, "Exiting on IOE", e);
               break;
            }
            catch(Throwable e)
            {
               handleStop(msg, "Failed to write msgType:" + msg, e);
               break;
            }
         }
         log.debug("End WriteTask.run");
      }

      /**
       * Stop the write thread
       */
      private void handleStop(BaseMsg msg, String error, Throwable e)
      {
         synchronized(running)
         {
            writeState = STOPPING;
            running = new Boolean(false);
         }

         if (e instanceof InterruptedException || e instanceof IOException)
         {
            if (trace)
               log.trace(error, e);
         }
         else
            log.error(error, e);

         if (msg != null)
         {
            msg.setError(e);
            synchronized( msg )
            {
               msg.notify();
            }
         }

         synchronized(running)
         {
            writeState = STOPPED;
            if (readState == STARTED)
            {
               readState = STOPPING;
               readThread.interrupt();
            }
         }

         try
         {
            out.close();
         }
         catch(Exception ignored)
         {
            if (trace)
               log.trace(ignored.getMessage(), ignored);
         }

         try
         {
            socket.close();
         }
         catch(Exception ignored)
         {
            if (trace)
               log.trace(ignored.getMessage(), ignored);
         }
      }
   }
}
