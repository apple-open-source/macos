/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm.jdbc;

import java.io.IOException;
import java.io.RandomAccessFile;

import java.io.Serializable;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.Statement;
import java.sql.SQLException;
import java.util.Iterator;
import java.util.TreeSet;

import javax.jms.JMSException;
import javax.sql.DataSource;

import org.jboss.mq.SpyJMSException;

/**
 *  This is used to keep a log of active transactions. It is used to rollback
 *  transactions when the system restarts.
 *
 * @created    August 16, 2001
 * @author:    Jayesh Parayali (jayeshpk1@yahoo.com)
 * @version    $Revision: 1.5 $
 */
public class TxLog {

   /////////////////////////////////////////////////////////////////////
   // Attributes
   /////////////////////////////////////////////////////////////////////
   protected long   nextTransactionId = Long.MIN_VALUE;
   private final DataSource ds;
   private String transactionTableName;

   /////////////////////////////////////////////////////////////////////
   // Constructors
   /////////////////////////////////////////////////////////////////////
   public TxLog( DataSource ds, String transactionTableName ) throws JMSException
   {
      if (ds == null)
      {
         throw new IllegalArgumentException("Must supply a datasource to construct a TxLog");
      }
      this.ds = ds;
      this.transactionTableName = transactionTableName;
      try
      {
         Connection c = ds.getConnection();
         try
         {
            ResultSet rs = c.getMetaData().getTables(null, null, this.transactionTableName, null);
            if (!rs.next())
            {
               Statement s = c.createStatement();
               try
               {
                  s.executeUpdate("create table " + this.transactionTableName + " (id varchar(32) primary key)");
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
         throwJMSException("could not find or set up transaction table", e);
      } // end of try-catch

   }


   public synchronized TreeSet restore()
      throws JMSException {
      TreeSet items = new TreeSet();
      Connection con = null;
      PreparedStatement stmt = null;
      ResultSet rs = null;
      try
      {
         try
         {
            con = getConnection();
            try
            {
               stmt = con.prepareStatement( "select id from " + transactionTableName );
               try
               {
                  rs = stmt.executeQuery();
                  while ( rs.next() )
                  {
                     long id = Long.parseLong( rs.getString( 1 ).trim(), 16 );
                     items.add( new Long( id ) );
                  }
               }
               finally
               {
                  rs.close();
               } // end of finally
            }
            finally
            {
               stmt.close();
            } // end of finally
         }
         finally
         {
            con.close();
         } // end of finally
      }
      catch ( SQLException e )
      {
         throwJMSException( "Could not write transaction log on commit.", e );
      }
      return items;
   }

   public synchronized void commitTx( org.jboss.mq.pm.Tx txId )
      throws JMSException {
      Connection con = null;
      PreparedStatement stmt = null;
      try {
         con = getConnection();
         stmt = con.prepareStatement( "delete from " + transactionTableName + " where id = ?" );
         String hexString = null;
         long id = txId.longValue();
         if ( id <= 0L ) {
            hexString = "-" + Long.toHexString( ( -1 ) * id );
         } else {
            hexString = Long.toHexString( id );
         }

         stmt.setString( 1, hexString );
         stmt.executeUpdate();
         con.commit();
      } catch ( SQLException e ) {
         throwJMSException( "Could not write transaction log on commit.", e );
      }
      try {
         if ( stmt != null ) {
            stmt.close();
         }
         if ( con != null ) {
            con.close();
         }
      } catch ( SQLException e ) {
         throwJMSException( "Could not close database connection in transaction log (commitTx)", e );
      }
   }

   public synchronized org.jboss.mq.pm.Tx createTx()
      throws JMSException {
      org.jboss.mq.pm.Tx id = new org.jboss.mq.pm.Tx( nextTransactionId++ );
      Connection con = null;
      PreparedStatement stmt = null;
      try {
         con = getConnection();
         stmt = con.prepareStatement( "insert into " + transactionTableName + " values(?)" );
         String hexString = null;
         long lId = id.longValue();
         if ( lId <= 0L ) {
            hexString = "-" + Long.toHexString( ( -1 ) * lId );
         } else {
            hexString = Long.toHexString( lId );
         }

         stmt.setString( 1, hexString );
         stmt.executeUpdate();
         con.commit();
      } catch ( SQLException e ) {
         throwJMSException( "Could not write transaction log on commit.", e );
      }
      try {
         if ( stmt != null ) {
            stmt.close();
         }
         if ( con != null ) {
            con.close();
         }
      } catch ( SQLException e ) {
         throwJMSException( "Could not close database connection in transaction log (createTx).", e );
      }
      return id;
   }

   public synchronized void rollbackTx( org.jboss.mq.pm.Tx txId )
      throws JMSException {
      Connection con = null;
      PreparedStatement stmt = null;
      try {
         con = getConnection();
         stmt = con.prepareStatement( "delete from " + transactionTableName + " where id = ?" );
         String hexString = null;
         long id = txId.longValue();
         if ( id <= 0L ) {
            hexString = "-" + Long.toHexString( ( -1 ) * id );
         } else {
            hexString = Long.toHexString( id );
         }

         stmt.setString( 1, hexString );
         stmt.executeUpdate();
         con.commit();
      } catch ( SQLException e ) {
         throwJMSException( "Could not write transaction log on commit.", e );
      }
      try {
         if ( stmt != null ) {
            stmt.close();
         }
         if ( con != null ) {
            con.close();
         }
      } catch ( SQLException e ) {
         throwJMSException( "Could not close database connection in transaction log (rollbackTx)", e );
      }
   }

   private final Connection getConnection()
      throws SQLException {
      return ds.getConnection();
   }

   /////////////////////////////////////////////////////////////////////
   // Private Methods
   /////////////////////////////////////////////////////////////////////
   private void throwJMSException( String message, Exception e )
      throws JMSException {
      JMSException newE = new SpyJMSException( message );
      newE.setLinkedException( e );
      throw newE;
   }
}
