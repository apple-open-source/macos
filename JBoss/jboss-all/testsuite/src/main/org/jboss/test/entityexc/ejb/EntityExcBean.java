/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.entityexc.ejb;

import java.util.Collection;

import java.rmi.RemoteException;

import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.CreateException;
import javax.ejb.DuplicateKeyException;
import javax.ejb.FinderException;
import javax.ejb.ObjectNotFoundException;
import javax.ejb.NoSuchEntityException;
import javax.ejb.EJBException;

import javax.naming.InitialContext;
import javax.naming.Context;
import javax.naming.NamingException;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
 
import javax.sql.DataSource;

import org.jboss.test.entityexc.interfaces.*;

/**
 *  Implementation of the ExtityExc EJB.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
public class EntityExcBean implements EntityBean
{
    static org.apache.log4j.Category log =
       org.apache.log4j.Category.getInstance(EntityExcBean.class);
   
   static final private boolean debug = true;

   /**
    *  The entity context of this instance.
    */
   private EntityContext ctx;

   /**
    *  Flags that this instance should have been discarded
    *  by the container.
    */
   private boolean wasDiscarded = false;

   /**
    *  The primary key of this instance.
    *  The second instance property <code>value</code> is not
    *  stored here, but read and written on demand.
    */
   private int id;


   /**
    *  Check if we are called when we should have been discarded.
    */
   private void checkDiscarded()
   {
      if (wasDiscarded) {
         log.debug("**************************************************");
         log.debug("Attempt to invoke method on an instance " +
                            "that should have been discarded.");
         log.debug("**************************************************");
         throw new RuntimeException("Invokation on discarded instance");
      }
   }


   private void doFailure(boolean isAfter, int flags)
      throws MyAppException, CreateException
   {
      if (isAfter && (flags & EntityExc.F_SETROLLBACKONLY) != 0) {
         if (debug)
            log.debug("Marking transaction for rollback only.");
         ctx.setRollbackOnly();
      }

      if ((flags & EntityExc.F_EXC_MASK) != 0) {
         if (isAfter && (flags & EntityExc.F_THROW_BEFORE) != 0)
            return;
         if (!isAfter && (flags & EntityExc.F_THROW_BEFORE) == 0)
            return;

         switch (flags & EntityExc.F_EXC_MASK) {
            case EntityExc.EXC_MYAPPEXCEPTION:
               if (debug)
                  log.debug("Throwing MyAppException");
               throw new MyAppException(EntityExc.EXCEPTION_TEXT);
            case EntityExc.EXC_CREATEEXCEPTION:
               if (debug)
                  log.debug("Throwing CreateException");
               throw new CreateException(EntityExc.EXCEPTION_TEXT);
            case EntityExc.EXC_EJBEXCEPTION:
               if (debug)
                  log.debug("Throwing EJBException");
               wasDiscarded = true;
               throw new EJBException(EntityExc.EXCEPTION_TEXT);
            default:
               wasDiscarded = true;
               throw new EJBException("Unknown exception code.");
         }
      }
   }

   private void doFailureOnlyAppExc(boolean isAfter, int flags)
      throws MyAppException
   {
      try {
         doFailure(isAfter, flags);
      } catch (CreateException ex) {
         // should not happen
         wasDiscarded = true;
         throw new EJBException("Unexpected CreateException");
      }
   }

   public void setEntityContext(EntityContext ctx)
   {
      if (debug)
         log.debug("EntityExcBean.setEntityContext() entered.");

      checkDiscarded();
      this.ctx = ctx;
   }

   public void unsetEntityContext()
   {
      if (debug)
         log.debug("EntityExcBean.unsetEntityContext() entered.");

      checkDiscarded();
      ctx = null;
   }


   public void ejbActivate()
   {
      if (debug)
         log.debug("EntityExcBean.ejbActivate() entered.");

      checkDiscarded();
   }
 
   public void ejbPassivate()
   {
      if (debug)
         log.debug("EntityExcBean.ejbPassivate() entered.");

      checkDiscarded();
   }


   /**
    *  Get a reference to the environment of this enterprise bean.
    */
   protected Context getEnvironment()
   {
      try {
         Context ic = new InitialContext();
         return (Context)ic.lookup("java:comp/env");
      } catch (NamingException ex) {
         wasDiscarded = true;
         throw new EJBException(ex);
      }
   }

   /**
    *  Return the data source of this enterprise bean.
    */
   protected DataSource getDataSource()
   {
      try {
         return (DataSource)getEnvironment().lookup("jdbc/entityexc");
      } catch (NamingException ex) {
         wasDiscarded = true;
         throw new EJBException(ex);
      }
   }

   private Integer ejbCreate(Integer pk)
      throws CreateException
   { 
      if (debug)
         log.debug("EntityExcBean.ejbCreate(Integer pk=" +
                          pk.intValue() + ") entered.");

      this.id = pk.intValue();

      try {
         Connection conn = getDataSource().getConnection();
         try {
            PreparedStatement stmt;
 
            // Check for duplicate key
            stmt = conn.prepareStatement("select id " +
                                      "from entityexc " +
                                      "where id=?");
            try {
              stmt.setInt(1, id);
              ResultSet rs = stmt.executeQuery();
              try {
                 if (rs.next())
                    throw new DuplicateKeyException("EntityExc id " + pk.intValue() +
                                                    " already in database.");
               } finally {
                  rs.close();
               }
            } finally {
               stmt.close();
            }

            stmt = conn.prepareStatement("insert into entityexc (id, val) " +
                                      "values (?, ?)");
            try {
               stmt.setInt(1, id);
               stmt.setInt(2, 0);
               stmt.executeUpdate();
            } finally {
               stmt.close();
            }
 
         } finally {
            conn.close();
         }
      } catch (SQLException ex) {
         wasDiscarded = true;
         throw new EJBException(ex);
      }

      return pk;
   }
   
   private void ejbPostCreate(Integer pk)
   {
      if (debug)
         log.debug("EntityExcBean.ejbPostCreate(Integer pk=" +
                            pk.intValue() + ") entered.");
   }
 
   public Integer ejbCreate(Integer pk, int flags)
      throws MyAppException, CreateException
   { 
      if (debug)
         log.debug("EntityExcBean.ejbCreate(Integer pk=" +
                            pk.intValue() + ", int flags=0x" +
                            Integer.toHexString(flags) + ") entered.");

      checkDiscarded();

      if ((flags & EntityExc.F_FAIL_POSTCREATE) == 0)
         doFailure(false, flags);

      Integer pk2 = ejbCreate(pk);

      if ((flags & EntityExc.F_FAIL_POSTCREATE) == 0)
         doFailure(true, flags);

      return pk2;
   }

   public void ejbPostCreate(Integer pk, int flags)
      throws MyAppException, CreateException
   {
      if (debug)
         log.debug("EntityExcBean.ejbPostCreate(Integer pk=" +
                            pk.intValue() + ", int flags=" +
                            Integer.toHexString(flags) + ") entered.");

      checkDiscarded();

log.debug("#1");
      if ((flags & EntityExc.F_FAIL_POSTCREATE) != 0)
         doFailure(false, flags);

log.debug("#2");
      if ((flags & EntityExc.F_FAIL_POSTCREATE) != 0)
         doFailure(true, flags);
log.debug("#3");
   }
 

   public void ejbRemove()
   {
      if (debug)
         log.debug("EntityExcBean.ejbRemove() entered.");

      try {
         Connection conn = getDataSource().getConnection();
         try {
            PreparedStatement stmt;
 
            stmt = conn.prepareStatement("delete from entityexc " +
                                         "where id=?");
            try {
               stmt.setInt(1, id);
               stmt.executeUpdate();
            } finally {
               stmt.close();
            }
         } finally {
            conn.close();
         }
      } catch (SQLException ex) {
         wasDiscarded = true;
         throw new EJBException(ex);
      }
   }


   private Integer ejbFindByPrimaryKey(Integer pk)
      throws FinderException
   {
      if (debug)
         log.debug("EntityExcBean.ejbFindByPrimaryKey(Integer pk=" +
                            pk.intValue() + ") entered.");

      try {
         Connection conn = getDataSource().getConnection();
         try {
            PreparedStatement stmt;
 
            stmt = conn.prepareStatement("select id from entityexc where id=?");
            try {
               stmt.setInt(1, pk.intValue());
               ResultSet rs = stmt.executeQuery();
               try {
                  if (!rs.next())
                     throw new ObjectNotFoundException
		       ("EntityExc id " + pk.intValue() + " not found in database.");
               } finally {
                  rs.close();
               }
            } finally {
               stmt.close();
            }
         } finally {
            conn.close();
         }
      } catch (SQLException e) {
         throw new FinderException("Failed to execute query " +e);
      }
 
      return pk;
   }

   public Integer ejbFindByPrimaryKey(Integer pk, int flags)
      throws MyAppException, FinderException
   {
      if (debug)
         log.debug("EntityExcBean.ejbFindByPrimaryKey(Integer pk=" +
                            pk.intValue() + ", int flags=0x" +
                            Integer.toHexString(flags) + ") entered.");

      checkDiscarded();

      doFailureOnlyAppExc(false, flags);

      Integer pk2 = ejbFindByPrimaryKey(pk);

      doFailureOnlyAppExc(true, flags);

      return pk2;
   }

   private Collection ejbFindAll()
      throws FinderException
   {
      if (debug)
          log.debug("EntityExcBean.ejbFindAll() entered.");

      Collection c = new java.util.LinkedList();
      try {
         Connection conn = getDataSource().getConnection();
         try {
            PreparedStatement stmt;
 
            stmt = conn.prepareStatement("select id from entityexc");
            try {
               ResultSet rs = stmt.executeQuery();
               try {
                  while (rs.next())
                     c.add(new Integer(rs.getInt(1)));
               } finally {
                  rs.close();
               }
            } finally {
               stmt.close();
            }
         } finally {
            conn.close();
         }
      } catch (SQLException ex) {
         throw new FinderException("Failed to execute query " + ex);
      }
 
      return c;
   }

   public Collection ejbFindAll(int flags)
      throws MyAppException, FinderException
   {
      if (debug)
         log.debug("EntityExcBean.ejbFindAll(int flags=0x" +
                            Integer.toHexString(flags) + ") entered.");

      checkDiscarded();

      doFailureOnlyAppExc(false, flags);

      Collection c = ejbFindAll();

      doFailureOnlyAppExc(true, flags);

      return c;
   }

   public void ejbLoad() {
      if (debug)
         log.debug("EntityExcBean.ejbLoad() entered.");

      checkDiscarded();

      Object key = ctx.getPrimaryKey();
      if (key == null) 
         log.debug("EntityExcBean.ejbLoad(): " +
                            "ctx.getPrimaryKey() returned null.");
      else
         log.debug("EntityExcBean.ejbLoad(): " +
                            "ctx.getPrimaryKey() returned class " +
                            key.getClass().getName());

      id = ((Integer)ctx.getPrimaryKey()).intValue();
   }

   public void ejbStore() {
      if (debug)
         log.debug("EntityExcBean.ejbStore() entered.");

      checkDiscarded();
   }


   public void ejbHomeResetDatabase()
   {
      try {
         Connection conn = getDataSource().getConnection();
         try {
            log.debug("Creating database table entityexc.");

            PreparedStatement stmt;
 
            stmt = conn.prepareStatement("drop table entityexc");
            try {
               stmt.executeUpdate();
            } finally {
               stmt.close();
            }
            log.debug("Database table entityexc dropped.");
         } finally {
            conn.close();
         }
      } catch (SQLException ex) {
         log.debug("Ignoring error dropping database table: " + ex);
      }
      try {
         Connection conn = getDataSource().getConnection();
         try {
            log.debug("Creating database table entityexc.");

            PreparedStatement stmt;
 
            stmt = conn.prepareStatement("create table entityexc" +
                                         " (id integer, val integer)");
            try {
               stmt.executeUpdate();
            } finally {
               stmt.close();
            }
            log.debug("Database table entityexc created.");
         } finally {
            conn.close();
         }
      } catch (SQLException ex) {
         log.debug("Error creating database table: " + ex);
         wasDiscarded = true;
         throw new EJBException("Error creating database table: " + ex);
      }
   }


   //
   // Business method helpers.
   //

   /**
    *  Read the <code>val</code> property from the database,
    *  using the given connection.
    */
   private int get_val(Connection conn)
      throws SQLException
   {
      if (debug)
         log.debug("EntityExcBean.get_val() entered.");

      PreparedStatement stmt = conn.prepareStatement("select val " +
                                                     "from entityexc " +
                                                     "where id=?");
      try {
         stmt.setInt(1, id);
         ResultSet rs = stmt.executeQuery();
         try {
            if (rs.next() == false)
               throw new NoSuchEntityException("EntityExc id " + id +
                                          " not found in database.");
            int ret = rs.getInt(1);

            if (debug)
               log.debug("EntityExcBean.get_val() returning " + ret);

            return ret;
         } finally {
            rs.close();
         }
      } finally {
         stmt.close();
      }
   }

   /**
    *  Write the <code>val</code> property to the database,
    *  using the given connection.
    */
   private void set_val(int val, Connection conn)
      throws SQLException
   {
      if (debug)
         log.debug("EntityExcBean.set_val(" + val + ") entered.");

      PreparedStatement stmt = conn.prepareStatement("update entityexc " +
                                                     "set val=? " +
                                                     "where id=?");
      try {
         stmt.setInt(1, val);
         stmt.setInt(2, id);
         stmt.executeUpdate();
      } finally {
         stmt.close();
      }
   }

   //
   // Business methods.
   //

   public int getId()
   {
      if (debug)
         log.debug("EntityExcBean.getId() entered.");

      checkDiscarded();

      return id;
   }

   public int getVal()
   {
      if (debug)
         log.debug("EntityExcBean.getVal() entered.");

      checkDiscarded();

      try {
         Connection conn = getDataSource().getConnection();

         try {
            return get_val(conn);
         } finally {
            conn.close();
         }
      } catch (SQLException ex) {
         wasDiscarded = true;
         throw new EJBException(ex);
      }
   }

   public void incrementVal()
   {
      if (debug)
         log.debug("EntityExcBean.incrementVal(void) entered.");

      checkDiscarded();

      try {
         Connection conn = getDataSource().getConnection();

         try {
            set_val(get_val(conn) + 1, conn);
         } finally {
            conn.close();
         }
      } catch (SQLException ex) {
         wasDiscarded = true;
         throw new EJBException(ex);
      }
   }

   public void incrementVal(int flags)
      throws MyAppException
   {
      if (debug)
         log.debug("EntityExcBean.incrementVal(flags=0x" +
                            Integer.toHexString(flags) + ") entered.");

      checkDiscarded();

      doFailureOnlyAppExc(false, flags);

      incrementVal();

      doFailureOnlyAppExc(true, flags);
   }

}
