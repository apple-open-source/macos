/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm.jdbc;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.Statement;
import java.sql.SQLException;
import javax.jms.JMSException;
import javax.sql.DataSource;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.server.MessageCache;
import org.jboss.mq.server.MessageReference;

/**
 * This is used to keep SpyMessages on the disk and is used reconstruct the
 * queue in case of provider failure.
 *
 * @author: Jayesh Parayali (jayeshpk1@yahoo.com)
 * @version $Revision: 1.10.4.1 $
 */
public class MessageLog {

   /////////////////////////////////////////////////////////////////////
   // Attributes
   /////////////////////////////////////////////////////////////////////
   //maybe this will work with hypersonic??
   private static boolean SUPPORTS_OBJECTS = true;

   private final DataSource datasource;

   private final MessageCache messageCache;

   private final String messageTableName;

   MessageLog(MessageCache messageCache, javax.sql.DataSource datasource,
              String messageTableName)
      throws JMSException
   {
      if (messageCache == null)
      {
         throw new IllegalArgumentException("Need a MessageCache to construct a MessageLog!");
      } // end of if ()

      if (datasource == null)
      {
         throw new IllegalArgumentException("Need a datasource to construct a MessageLog!");
      } // end of if ()

      this.messageCache = messageCache;
      this.datasource = datasource;
      this.messageTableName = messageTableName;

      try
      {
         Connection c = datasource.getConnection();
         try
         {
            ResultSet rs = c.getMetaData().getTables(null, null, this.messageTableName, null);
            if (!rs.next())
            {
               Statement s = c.createStatement();
               try
               {
                  s.executeUpdate("create table " + this.messageTableName + " (destination varchar(32), messageblob object, messageid varchar(32))");
               }
               finally
               {
                  s.close();
               } // end of try-catch
            } // end of if ()
            rs.close();
         }
         finally
         {
            c.close();
         } // end of try-catch
      }
      catch (SQLException e)
      {
         throwJMSException("could not find or set up message table", e);
      } // end of try-catch

   }

   /////////////////////////////////////////////////////////////////////
   // Public Methods
   /////////////////////////////////////////////////////////////////////
   public void close() throws JMSException {
   }

   public Map restoreAll() throws JMSException {
      //WTF is 21???
      //String destin= dest.substring(21, dest.length());

      Map unrestoredMessages = new HashMap();

      //TreeMap messageIndex= new TreeMap();
      PreparedStatement pstmt= null;
      ResultSet rs= null;
      Connection con= null;

      try
      {
         try
         {
            con= datasource.getConnection();
            try
            {

               pstmt= con.prepareStatement("select destination, messageblob, messageid from " + messageTableName);
               //pstmt.setString(1, destin);
               try
               {
                  rs= pstmt.executeQuery();
                  while (rs.next())
                  {
                     String dest = rs.getString(1);
                     SpyMessage message = null;
                     if (SUPPORTS_OBJECTS)
                     {
                        message = (SpyMessage)rs.getObject(2);
                     } // end of if ()
                     else
                     {
                        byte[] st= (byte[]) rs.getObject(2);
                        ByteArrayInputStream baip= new ByteArrayInputStream(st);
                        ObjectInputStream ois= new ObjectInputStream(baip);
                        // re-create the object
                        message = (SpyMessage) ois.readObject();
                     } // end of else

                     //restore the messageId which is not persistent.
                     //ID stored in hexadecimal string!!
                     message.header.messageId= Long.parseLong(rs.getString(3).trim(), 16);
                     Long msgId= new Long(message.header.messageId);
                     MessageReference mr = messageCache.add(message);
                     Map messageIndex = (Map)unrestoredMessages.get(dest);
                     if (messageIndex == null)
                     {
                        messageIndex = new TreeMap();
                        unrestoredMessages.put(dest, messageIndex);
                     } // end of if ()

                     messageIndex.put(msgId, mr);
                  }
               }
               finally
               {
                  rs.close();
               } // end of finally
            }
            finally
            {
               pstmt.close();
            } // end of finally
         }
         finally
         {
            con.close();
         } // end of finally

      }
      catch (SQLException e)
      {
         throwJMSException("SQL error while rebuilding the tranaction log.", e);
      }
      catch (Exception e)
      {
         throwJMSException("Could not rebuild the queue from the queue's tranaction log.", e);
      }
      return unrestoredMessages;
   }

   private void throwJMSException(String message, Exception e) throws JMSException {
      JMSException newE= new SpyJMSException(message);
      newE.setLinkedException(e);
      throw newE;
   }

   public void add(SpyMessage message, org.jboss.mq.pm.Tx transactionId) throws JMSException
   {
      PreparedStatement pstmt= null;
      Connection con= null;

      try
      {
         con= datasource.getConnection();
         pstmt= con.prepareStatement("insert into " + messageTableName + " (messageid, destination, messageblob) VALUES(?,?,?)");
         String hexString= null;
         if (message.header.messageId <= 0)
         {
            hexString= "-" + Long.toHexString((-1) * message.header.messageId);
         }
         else
         {
            hexString= Long.toHexString(message.header.messageId);
         } // end of else
         pstmt.setString(1, hexString);
         pstmt.setString(2, ((SpyDestination) message.getJMSDestination()).getName());
         setBlob(pstmt, 3, message);

         pstmt.executeUpdate();

         pstmt.close();
      }
      catch (IOException e)
      {
         throwJMSException("Could serialize the message.", e);
      }
      catch (SQLException e)
      {
         throwJMSException("Could not write message to the database.", e);
      }
      finally
      {
         try
         {
            //if (pstmt != null)
            //pstmt.close();
            if (con != null)
               con.close();
         }
         catch (SQLException e)
         {
            throwJMSException("Could not close the database.", e);
         }

      }
   }

   public void setBlob(PreparedStatement pstmt, int column, SpyMessage message)
      throws SQLException, IOException
   {
      if (SUPPORTS_OBJECTS)
      {
         pstmt.setObject(column, message);
      } // end of if ()
      else
      {
         ByteArrayOutputStream baos= new ByteArrayOutputStream();
         ObjectOutputStream oos= new ObjectOutputStream(baos);
         oos.writeObject(message);
         byte[] messageAsBytes= baos.toByteArray();
         ByteArrayInputStream bais= new ByteArrayInputStream(messageAsBytes);
         pstmt.setBinaryStream(column, bais, messageAsBytes.length);
      } // end of else
   }

   public javax.sql.DataSource getDatasource() {
      return datasource;
   }

   public void remove(SpyMessage message, org.jboss.mq.pm.Tx transactionId)
      throws JMSException
   {
      PreparedStatement pstmt= null;
      Connection con= null;
      try
      {
         con= datasource.getConnection();
         pstmt= con.prepareStatement("delete from " + messageTableName + " where messageid = ? and destination = ?");
         String hexString= null;
         if (message.header.messageId <= 0)
         {
            hexString= "-" + Long.toHexString((-1) * message.header.messageId);
         }
         else
         {
            hexString= Long.toHexString(message.header.messageId);
         }
         pstmt.setString(1, hexString);
         pstmt.setString(2, ((SpyDestination) message.getJMSDestination()).getName().trim());

         pstmt.execute();
      }
      catch (SQLException e)
      {
         throwJMSException("Could not remove the message.", e);
      }
      finally
      {
         try
         {
            if (pstmt != null)
               pstmt.close();
            if (con != null)
               con.close();
         }
         catch (SQLException e)
         {
            throwJMSException("Could not close the database.", e);
         }

      }
   }

   public void update(SpyMessage message, org.jboss.mq.pm.Tx transactionId)
      throws JMSException
   {
      PreparedStatement pstmt= null;
      Connection con= null;
      try
      {
         con= datasource.getConnection();
         pstmt= con.prepareStatement("update " + messageTableName + " set messageblob = ? where messageid = ? and destination = ?");
         String hexString= null;
         if (message.header.messageId <= 0)
         {
            hexString= "-" + Long.toHexString((-1) * message.header.messageId);
         }
         else
         {
            hexString= Long.toHexString(message.header.messageId);
         } // end of else
         setBlob(pstmt, 1, message);
         pstmt.setString(2, hexString);
         pstmt.setString(3, ((SpyDestination) message.getJMSDestination()).getName());

         pstmt.executeUpdate();

         pstmt.close();
      }
      catch (IOException e)
      {
         throwJMSException("Could serialize the message.", e);
      }
      catch (SQLException e)
      {
         throwJMSException("Could not update message in the database.", e);
      }
      finally
      {
         try
         {
            if (con != null)
               con.close();
         }
         catch (SQLException e)
         {
            throwJMSException("Could not close the database.", e);
         }

      }
   }

}
