/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm.file;



import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.Map;
import java.util.TreeMap;
import javax.jms.JMSException;

import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.server.JMSDestinationManager;
import org.jboss.mq.server.MessageCache;
import org.jboss.mq.server.MessageReference ;

import org.jboss.logging.Logger;

/**
 *  This is used to keep SpyMessages on the disk and is used reconstruct the
 *  queue in case of provider failure.
 *
 * @created    August 16, 2001
 * @author:    Paul Kendall (paul.kendall@orion.co.nz)
 * @version    $Revision: 1.14.2.1 $
 */
public class MessageLog {

   /////////////////////////////////////////////////////////////////////
   // Attributes
   /////////////////////////////////////////////////////////////////////
   private File queueName;

   private MessageCache messageCache;

   private static Logger log = Logger.getLogger( MessageLog.class );

   /////////////////////////////////////////////////////////////////////
   // Constants
   /////////////////////////////////////////////////////////////////////

   /////////////////////////////////////////////////////////////////////
   // Constructor
   /////////////////////////////////////////////////////////////////////
   public MessageLog(MessageCache messageCache, File file )
      throws JMSException
   {
      if (messageCache == null)
      {
         throw new IllegalArgumentException("Need a MessageCache to construct a MessageLog!");
      } // end of if ()

      this.messageCache = messageCache;
      queueName = file;
      if( !queueName.exists() ) {
         if( !queueName.mkdirs() ) {
            throw new JMSException("Could not create the directory: "+queueName);
         }
      }
      log.debug("Message directory created: "+queueName);
   }

   /////////////////////////////////////////////////////////////////////
   // Public Methods
   /////////////////////////////////////////////////////////////////////
   public void close()
      throws JMSException
   {
   }

   public Map restore( java.util.TreeSet rollBackTXs )
      throws JMSException
   {
      //use sorted map to get queue order right.
      TreeMap messageIndex = new TreeMap();

      try
      {
         File[] files = queueName.listFiles();
         for ( int i = 0; i < files.length; i++ )
         {
            String fileName = files[i].getName();
            int extIndex = fileName.indexOf( "." );
            if ( extIndex < 0 )
            {
               //non transacted message so simply restore
               restoreMessageFromFile( messageIndex, files[i] );
            } else
            {
               //test if message from a transaction that is being rolled back.
               Long tx = new Long( Long.parseLong( fileName.substring( extIndex + 1 ) ) );
               if ( rollBackTXs.contains( tx ) )
               {
                  delete( files[i] );
               } else
               {
                  restoreMessageFromFile( messageIndex, files[i] );
               }
            }
         }
      } catch ( Exception e )
      {
         throwJMSException( "Could not rebuild the queue from the queue's tranaction log.", e );
      }
      return messageIndex;
   }

   public void add( MessageReference messageRef, org.jboss.mq.pm.Tx transactionId )
      throws JMSException
   {
      try
      {
         SpyMessage message = messageRef.getMessage();
         File f;
         String fileName = PersistenceManager.encodeFileName( message.getJMSMessageID() );
         if ( transactionId == null )
         {
            f = new File( queueName, fileName );
         } else
         {
            f = new File( queueName, fileName + "." + transactionId );
         }
         writeMessageToFile( message, f );
         messageRef.persistData = f;
      } catch ( IOException e )
      {
         throwJMSException( "Could not write to the tranaction log.", e );
      }
   }

   public void finishAdd( MessageReference message, org.jboss.mq.pm.Tx transactionId )
      throws JMSException
   {
   }

   public void finishRemove( MessageReference messageRef, org.jboss.mq.pm.Tx transactionId )
      throws JMSException
   {
      try
      {
         File file = ( File )messageRef.persistData;
         delete( file );
      } catch ( IOException e )
      {
         throwJMSException( "Could not write to the tranaction log.", e );
      }
   }

   public void remove( SpyMessage message, org.jboss.mq.pm.Tx transactionId )
      throws JMSException
   {
   }

   public void update( MessageReference messageRef, org.jboss.mq.pm.Tx transactionId )
      throws JMSException
   {
      try
      {
         /* Rename the old file out of the way first? The most likely failure is disk full? */
         SpyMessage message = messageRef.getMessage();
         File file = ( File )messageRef.persistData;
         writeMessageToFile( message, file );
      } catch ( IOException e )
      {
         throwJMSException( "Could not update the message.", e );
      }
   }

   public void finishUpdate( MessageReference messageRef, org.jboss.mq.pm.Tx transactionId )
      throws JMSException
   {
   }

   public void undoAdd( MessageReference messageRef, org.jboss.mq.pm.Tx transactionId )
      throws JMSException
   {
      try
      {
         File file = ( File )messageRef.persistData;
         delete( file );
      } catch ( IOException e )
      {
         throwJMSException( "Could not write to the tranaction log.", e );
      }
   }

   public void undoRemove( MessageReference message, org.jboss.mq.pm.Tx transactionId )
      throws JMSException
   {
   }

   /////////////////////////////////////////////////////////////////////
   // Utility Methods
   /////////////////////////////////////////////////////////////////////
   protected void delete( File file )
      throws IOException
   {
      // I know this looks silly!  But sometimes (but not often) M$ systems fail
      // on the first delete
      if ( !file.delete() )
      {
         Thread.yield();
         if ( file.exists() )
         {
            if ( !file.delete() )
            {
               log.warn( "Failed to delete file: " + file.getAbsolutePath() );
            }
         } else
         {
            if( log.isTraceEnabled() )
               log.trace( "File was deleted, but delete() failed for: " + file.getAbsolutePath() );
         }
      }
   }

   protected void rename( File from, File to )
      throws IOException
   {
      // I know this looks silly!  But sometimes (but not often) M$ systems fail
      // on the first rename (as above)
      if ( !from.renameTo( to ) )
      {
         Thread.yield();
         if ( from.exists() )
         {
            if ( !from.renameTo( to ) )
            {
               log.warn( "Rename of file " + from.getAbsolutePath() + " to " + to.getAbsolutePath() + " failed." );
            }
         } else 
         {
            if( log.isTraceEnabled() )
               log.trace( "Rename of file " + from.getAbsolutePath() + " to " + to.getAbsolutePath() + " failed but from no longer exists?" );
         }
      }
   }

   protected void writeMessageToFile( SpyMessage message, File file )
      throws IOException
   {
      ObjectOutputStream out = new ObjectOutputStream( new FileOutputStream( file ) );
      out.writeLong( message.header.messageId );
      SpyMessage.writeMessage(message,out);
      out.flush();
      out.close();
   }

   protected void restoreMessageFromFile(TreeMap store, File file )
      throws Exception
   {
      ObjectInputStream in = new ObjectInputStream( new FileInputStream( file ) );
      long msgId = in.readLong();
      SpyMessage message = SpyMessage.readMessage(in);
      in.close();
      message.header.messageId = msgId;

      MessageReference mr = messageCache.add(message);
      mr.persistData = file;
      store.put( new Long( msgId ), mr );
   }

   private void throwJMSException( String message, Exception e )
      throws JMSException
   {
      JMSException newE = new SpyJMSException( message );
      newE.setLinkedException( e );
      throw newE;
   }
      
   
}
/*
vim:tabstop=3:expandtab:shiftwidth=3
*/
