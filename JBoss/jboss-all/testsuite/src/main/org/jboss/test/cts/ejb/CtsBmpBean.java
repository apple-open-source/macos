package org.jboss.test.cts.ejb;

import java.util.Collection;

import java.rmi.RemoteException;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.Statement;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.ResultSetMetaData;
import java.sql.Types;
import java.sql.SQLException;

import javax.naming.InitialContext;
import javax.naming.Context;
import javax.naming.NamingException;

import javax.jms.JMSException;

import javax.ejb.*;

import org.jboss.test.cts.keys.AccountPK;

import org.jboss.test.cts.jms.ContainerMBox;
import org.jboss.test.cts.jms.MsgSender;

/**
 *  Class CtsBmpBean is a simple BMP entity bean for testing.
 *
 *  If the table used for persistence here does not exist, ejbFindAll()
 *  will create it.
 *
 *  @author $Author: user57 $
 *  @version $Revision: 1.7 $
 */

public class CtsBmpBean
   implements EntityBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
   private static final String TABLE_NAME = "BMP_BEAN_TBL";
   EntityContext               ctx        = null;
   DataSource                  ds         = null;

   private MsgSender ms = null;

   // bmp fields
   String accountNumber;
   String personsName;

   /**
    *  Create a new instance.
    *
    *  @param pk The primary key of the new instance.
    *  @param personsName Person name of the new instance.
    *
    *  @throws CreateException
    *          In case of database row creation failure.
    *  @throws DuplicateKeyException
    *          If another instance with this primary key already exist.
    */
   public AccountPK ejbCreate(AccountPK pk, String personsName)
      throws CreateException, DuplicateKeyException
   {
      log.debug("entry ejbCreate(\"" + pk.getKey() + "\", " +
                                         "\"" + personsName + "\")");

      sendMsg(ContainerMBox.EJB_CREATE_MSG);

      try {
        Connection con = ds.getConnection();
        try {
           PreparedStatement ps;
 
           // Check for duplicates.
           ps = con.prepareStatement("SELECT accountNumber " +
                                     "FROM " + TABLE_NAME + " " +
                                     "WHERE accountNumber=?");
           try {
              ps.setString(1, pk.getKey());

              ResultSet rs = ps.executeQuery();

              if (rs.next())
                 throw new DuplicateKeyException("Bean with accountNumber=" +
                                                 pk.getKey() +
                                                 " already exists.");
           } finally {
              ps.close();
           }
 
           // Create in database.
           ps = con.prepareStatement("INSERT INTO " + TABLE_NAME +
                                     " VALUES (?,?)");
           try {
              ps.setString(1, pk.getKey());
              ps.setString(2, personsName);

              ps.execute();
           } finally {
              ps.close();
           }
         } finally {
           con.close();
         }
      } catch (SQLException e) {
         log.debug("failed", e);

         throw new CreateException("Entity bean creation failure: " +
                                   e.getMessage());
      }

      this.accountNumber = pk.getKey();
      this.personsName   = personsName;

      log.debug("Created \"" + accountNumber + "\".");

      return pk;
   }

   /**
    *  Method ejbPostCreate
    */
   public void ejbPostCreate(AccountPK pk, String personsName)
   {
      log.debug("ejbPostCreate(AccountPK, String) called");

      sendMsg(ContainerMBox.EJB_POST_CREATE_MSG);
   }

   /**
    *  Find a single instance by primary key.
    *
    *  @param pk Primary key of the instance searched for.
    *
    *  @throws ObjectNotFoundException
    *          If no instance with this primary key exists.
    *  @throws FinderException
    *          If the lookup failed.
    */
   public AccountPK ejbFindByPrimaryKey(AccountPK pk)
      throws FinderException
   {
      log.debug("entry ejbFindByPrimaryKey");

      try {
        Connection con = ds.getConnection();
        try {
           PreparedStatement ps;
 
           ps = con.prepareStatement("SELECT accountNumber " +
                                     "FROM " + TABLE_NAME + " " +
                                     "WHERE accountNumber=?");
           try {
              ps.setString(1, pk.getKey());

              ResultSet rs = ps.executeQuery();

              if (!rs.next())
                 throw new ObjectNotFoundException("No bean with " +
                                                   "accountNumber=" +
                                                   pk.getKey() + " found.");

              return pk;
           } finally {
              ps.close();
           }
         } finally {
           con.close();
         }
      } catch (SQLException e) {
         log.debug("failed", e);

         throw new FinderException("Could not find: " + e.getMessage());
      }
   }

   /**
    *  Find all instances.
    *
    *  @throws FinderException
    *          If the lookup failed.
    */
   public Collection ejbFindAll()
      throws FinderException
   {
      log.debug("entry ejbFindAll");

      ensureTableExists();

      Collection result = new java.util.LinkedList();
      try {
         Connection con = ds.getConnection();
         try {
            PreparedStatement ps;
 
            ps = con.prepareStatement("SELECT accountNumber " +
                                      "FROM " + TABLE_NAME);
            try {
               ResultSet rs = ps.executeQuery();

               while (rs.next())
                  result.add(new AccountPK(rs.getString(1)));

               return result;
            } finally {
               ps.close();
            }
         } finally {
            con.close();
         }
      } catch (SQLException e) {
          log.debug("failed", e);

          throw new FinderException("Could not find: " + e.getMessage());
      }
   }

   /**
    *  Find all instances where the personsName property is
    *  equal to the argument.
    *
    *  @throws FinderException
    *          If the lookup failed.
    */
   public Collection ejbFindByPersonsName(String guysName)
      throws FinderException
   {
      log.debug("entry ejbFindByPersonsName(\"" + guysName + "\").");

      Collection result = new java.util.LinkedList();
      try {
         Connection con = ds.getConnection();
         try {
            PreparedStatement ps;
 
            ps = con.prepareStatement("SELECT accountNumber " +
                                      "FROM " + TABLE_NAME + " " +
                                      "WHERE name=?");
            try {
               ps.setString(1, guysName);

               ResultSet rs = ps.executeQuery();

               while (rs.next())
                  result.add(new AccountPK(rs.getString(1)));

               return result;
            } finally {
               ps.close();
            }
         } finally {
            con.close();
         }
      } catch (SQLException e) {
         log.debug("failed", e);

         throw new FinderException("Could not find: " + e.getMessage());
      }
   }

   /**
    *  Method ejbLoad
    */
   public void ejbLoad()
   {
      log.debug("ejbLoad(\"" +
                         ((AccountPK)ctx.getPrimaryKey()).getKey() +
                         "\") called");

      sendMsg(ContainerMBox.EJB_LOAD_MSG);

      try {
         Connection con = ds.getConnection();
         try {
            PreparedStatement ps;
 
            ps = con.prepareStatement("SELECT accountNumber,name " +
                                       "FROM " + TABLE_NAME + " " +
                                       "WHERE accountNumber=?");
            try {
               AccountPK pk = (AccountPK)ctx.getPrimaryKey();

               ps.setString(1, pk.getKey());
               ResultSet rs = ps.executeQuery();

               if (rs.next() == false)
                  throw new NoSuchEntityException("Instance " + pk.getKey() +
                                                  " not found in database.");

               accountNumber = rs.getString(1);
               personsName = rs.getString(2);
            } finally {
               ps.close();
            }
         } finally {
          con.close();
         }
      } catch (SQLException e) {
         log.debug("failed", e);

         throw new EJBException(e);
      }

      log.debug("ejbLoad(\"" +
                         ((AccountPK)ctx.getPrimaryKey()).getKey() +
                         "\") returning");

   }

   /**
    *  Method ejbStore
    */
   public void ejbStore()
   {
      log.debug("ejbStore(\"" + accountNumber + "\") called");
//Thread.currentThread().dumpStack();

      sendMsg(ContainerMBox.EJB_STORE_MSG);

      try {
         Connection con = ds.getConnection();
         try {
            PreparedStatement ps;
 
            ps = con.prepareStatement("UPDATE " + TABLE_NAME + " " +
                                       "SET name=? " +
                                       "WHERE accountNumber=?");
            try {
               ps.setString(1, personsName);
               ps.setString(2, accountNumber);
 
               ps.executeUpdate();
            } finally {
               ps.close();
            }
         } finally {
            con.close();
         }
      } catch (SQLException e) {
         log.debug("failed", e);

         throw new EJBException(e);
      }

      log.debug("ejbStore(\"" + accountNumber + "\") returning");
   }

   /**
    *  Method ejbRemove
    */
   public void ejbRemove()
   {
      log.debug("ejbRemove(\"" + accountNumber + "\") called");

      sendMsg(ContainerMBox.EJB_REMOVE_MSG);

      try {
         Connection con = ds.getConnection();
         try {
            PreparedStatement ps;
 
            ps = con.prepareStatement("DELETE FROM " + TABLE_NAME + " " +
                                       "WHERE accountNumber=?");
            try {
               ps.setString(1, accountNumber);
 
               ps.executeUpdate();
            } finally {
               ps.close();
            }
         } finally {
            con.close();
         }
      } catch (SQLException e) {
         log.debug("failed", e);

         throw new EJBException(e);
      }

      log.debug("Removed \"" + accountNumber + "\".");
   }

   /**
    *  Method ejbActivate
    */
   public void ejbActivate()
   {
      log.debug("ejbActivate() called");

      sendMsg(ContainerMBox.EJB_ACTIVATE_MSG);
   }

   /**
    *  Method ejbPassivate
    */
   public void ejbPassivate()
   {
      log.debug("ejbPassivate() called");

      sendMsg(ContainerMBox.EJB_PASSIVATE_MSG);

      // drop message sender
      if (ms != null) {
         try {
            ms.close();
         } catch (JMSException e) {
            log.debug("failed", e);
            // otherwise ignore
         }
         ms = null;
      }
   }

   /**
    *  Method setEntityContext
    */
   public void setEntityContext(EntityContext ctx)
   {
      log.debug("setEntityContext(\"" + ctx.getPrimaryKey()
                         + "\") called");

      sendMsg(ContainerMBox.SET_ENTITY_CONTEXT_MSG);

      this.ctx = ctx;

      // lookup the datasource
      try {
         Context context = new InitialContext();

         ds = (DataSource)context.lookup("java:comp/env/datasource");
      } catch (NamingException nex) {
         log.debug("failed", nex);

         throw new EJBException("Datasource not found: " + nex.getMessage());
      }
   }

   /**
    *  Method unsetEntityContext
    */
   public void unsetEntityContext()
   {
      log.debug("unsetEntityContext() called");

      sendMsg(ContainerMBox.UNSET_ENTITY_CONTEXT_MSG);

      ctx = null;
      ds = null;
   }


   //
   //  Private methods
   //

   /**
    *  Check if a good table exists, and create it if needed.
    */
   private void ensureTableExists()
   {
      boolean exists = true;
 
      try {
         Connection con = ds.getConnection();
         try {
            Statement s  = con.createStatement();
            try {
               ResultSet rs = s.executeQuery("SELECT * FROM " + TABLE_NAME);
               ResultSetMetaData md = rs.getMetaData();
 
               if (md.getColumnCount() != 2)
                 throw new SQLException("Not two columns");
 
               if (!"ACCOUNTNUMBER".equals(md.getColumnName(1).toUpperCase()))
                 throw new SQLException("First column name not \"accountNumber\"");
               if (!"NAME".equals(md.getColumnName(2).toUpperCase()))
                 throw new SQLException("Second column name not \"name\"");
 
               if (md.getColumnType(1) != Types.VARCHAR)
                 throw new SQLException("First column type not VARCHAR");
               if (md.getColumnType(2) != Types.VARCHAR)
                 throw new SQLException("Second column type not VARCHAR");
            } finally {
               s.close();
            }
         } finally {
            con.close();
         }
      } catch (SQLException e) {
        exists = false;
      }
 
      if (!exists)
         initializeDatabaseTable();
   }

   /**
    *  Create the table, removing any old table first.
    */
   private void initializeDatabaseTable()
   {
      log.debug("Initializing DATABASE tables for BMP test...");
 
      try {
         Connection con = ds.getConnection();
         try {
            Statement s;
 
            s = con.createStatement();
            try {
               s.executeUpdate("DROP TABLE " + TABLE_NAME);
               log.debug("Dropped old table.");
            } catch (SQLException e) {
               // Ignore: Presume the table didn't exist.
            } finally {
               s.close();
            }
 
            s = con.createStatement();
            try {
               s.executeUpdate("CREATE TABLE " + TABLE_NAME + " " +
                               "(accountNumber VARCHAR(25)," +
                               " name VARCHAR(200))");
               log.debug("Created new table.");
            } finally {
               s.close();
            }
         } finally {
            con.close();
         }
      } catch (SQLException e) {
         log.debug("failed", e);
 
         throw new EJBException(e);
      }
 
      log.debug("Initialized DATABASE tables for BMP test.");
   }

   /**
    *  Send a JMS message.
    */
   private void sendMsg(String msg)
   {
      if (ms == null)
         ms = new MsgSender();

      ms.sendMsg(msg);
   }

   //
   //  Business methods
   //

   /**
    *  Setter for property personsName.
    */
   public void setPersonsName(String personsName)
   {
      this.personsName = personsName;
   }

   /**
    *  Getter for property personsName.
    */
   public String getPersonsName()
   {
      return this.personsName;
   }
}

