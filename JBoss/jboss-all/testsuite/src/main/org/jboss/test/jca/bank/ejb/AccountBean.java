
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jca.bank.ejb;



import java.rmi.RemoteException;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.FinderException;
import javax.ejb.NoSuchEntityException;
import javax.ejb.ObjectNotFoundException;
import javax.sql.DataSource;

import javax.naming.InitialContext;
import java.util.ArrayList;
import java.sql.SQLException;

import org.apache.log4j.Category;


/**
 * Describe class <code>AccountBean</code> here.
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version 1.0
 * @ejb:bean   name="Account"
 *             jndi-name="Account"
 *             local-jndi-name="LocalAccount"
 *             view-type="both"
 *             type="BMP"
 *             primkey-field="id"
 * @ejb:pk class="java.lang.Integer"
 */
public class AccountBean
   implements EntityBean
{

   private Connection c;

   private Integer id;
   private int balance;
   private Integer customerId;    

   private EntityContext ctx;
   
   /**
    * Abstract cmp2 field get-set pair for field id
    * Get the value of id
    * @return value of id
    *
    * @ejb:interface-method
    */
   public Integer getId()
   {
      return id;
   }
   
   /**
    * Set the value of id
    * @param id  Value to assign to id
    *
    * @ejb:interface-method
    */
   public void setId(final Integer id)
   {
      this.id = id;
   }
   
   
   
   /**
    * field get-set pair for field balance
    * Get the value of balance
    * @return value of balance
    *
    * @ejb:interface-method
    */
   public int getBalance()
   {
      return balance;
   }
   
   /**
    * Set the value of balance
    * @param balance  Value to assign to balance
    *
    * @ejb:interface-method
    */
   public void setBalance(final int balance)
   {
      this.balance = balance;
   }
   
   
   
   /**
    * field get-set pair for field customerId
    * Get the value of customerId
    * @return value of customerId
    *
    * @ejb:interface-method
    */
   public Integer getCustomerId()
   {
      return customerId;
   }
   
   /**
    * Set the value of customerId
    * @param customerId  Value to assign to customerId
    *
    * @ejb:interface-method
    */
   public void setCustomerId(final Integer customerId)
   {
      this.customerId = customerId;
   }
   



   /**
    * Describe <code>deposit</code> method here.
    *
    * @param amount an <code>int</code> value
    * @ejb:interface-method
    */
   public void deposit(int amount)
   {
      setBalance(getBalance()+amount);
   }
   
   /**
    * Describe <code>withdraw</code> method here.
    *
    * @param amount an <code>int</code> value
    * @ejb:interface-method
    */
   public void withdraw(int amount)
   {
      setBalance(getBalance()-amount);
   }


   //ENTITY methods
   /**
    * Describe <code>ejbCreate</code> method here.
    *
    * @param id an <code>Integer</code> value
    * @param balance an <code>int</code> value
    * @param customerId an <code>Integer</code> value
    * @return an <code>Integer</code> value
    * @ejb:create-method
    */
   public Integer ejbCreate(final Integer id, final int balance, final Integer customerId) 
      throws CreateException
   { 
      setId(id);
      setBalance(balance);
      setCustomerId(customerId);
      PreparedStatement ps = null;
      try 
      {
      
         ps = getConnection().prepareStatement("INSERT INTO CCBMPACCOUNT (ID, BALANCE, CUSTOMERID) VALUES (?, ?, ?)");
         ps.setInt(1, id.intValue());
         ps.setInt(2, balance);
         ps.setObject(3, customerId);
      }
      catch (Exception e)
      {
         Category.getInstance(getClass().getName()).info("Exception in ejbCreate", e);
         throw new CreateException("Can't insert: " + e);
      } // end of try-catch
      finally
      {
         try 
         {
            if (ps != null) {ps.close();}
         }
         catch (Exception e)
         {
            Category.getInstance(getClass().getName()).info("Exception closing ps: " + e);
         } // end of try-catch
         
      } // end of finally
      return id;
   }
   
   public void ejbPostCreate(final Integer id, final int balance, final Integer customerId) 
   { 
   }

   /**
    * Describe <code>ejbFindByPrimaryKey</code> method here.
    *
    * @param id an <code>Integer</code> value
    * @return an <code>Integer</code> value
    * @exception FinderException if an error occurs
    * @ejb:finder
    */
   public Integer ejbFindByPrimaryKey(final Integer id)
      throws FinderException
   {
      PreparedStatement ps = null;
      try 
      {
         ps = getConnection().prepareStatement("SELECT ID FROM CCBMPACCOUNT WHERE ID = ?");
         ps.setInt(1, id.intValue());
         ResultSet rs = ps.executeQuery();
         if (!rs.next()) 
         {
            throw new ObjectNotFoundException("No such account: " + id);
         } // end of if ()
         rs.close();

      }
      catch (Exception e)
      {
         Category.getInstance(getClass().getName()).info("Exception in findByPK", e);
         throw new EJBException("Problem in findByPrimaryKey: " + e);
      } // end of try-catch
      finally
      {
         try 
         {
            if (ps != null) {ps.close();}
         }
         catch (Exception e)
         {
            Category.getInstance(getClass().getName()).info("Exception closing ps: " + e);
         } // end of try-catch
      } // end of finally
      return id;
   }

   /**
    * Describe <code>ejbFindByCustomerId</code> method here.
    *
    * @param customerId an <code>Integer</code> value
    * @return a <code>Collection</code> value
    * @ejb:finder-method
    */
   public Collection ejbFindByCustomerId(final Integer customerId)
   {
      PreparedStatement ps = null;
      try 
      {
         ps = getConnection().prepareStatement("SELECT ID FROM CCBMPACCOUNT WHERE CUSTOMERID = ?");
         ps.setInt(1, customerId.intValue());
         ResultSet rs = ps.executeQuery();
         Collection result = new ArrayList();
         while (rs.next()) 
         {
            result.add(new Integer(rs.getInt(1)));
         } // end of if ()
         rs.close();
            return result;
      }
      catch (Exception e)
      {
         Category.getInstance(getClass().getName()).info("Exception in findbyCustomerID", e);
         throw new EJBException(e);
      } // end of try-catch
      finally
      {
         try 
         {
            if (ps != null) {ps.close();}
         }
         catch (Exception e)
         {
            Category.getInstance(getClass().getName()).info("Exception closing ps: " + e);
         } // end of try-catch
      } // end of finally
   }

   public void ejbActivate()
   {
   }
   
   public void ejbPassivate()
   {
      if (c != null) 
      {
         try 
         {
            c.close();
         }
         catch (Exception e)
         {
            Category.getInstance(getClass().getName()).info("Exception closing c: " + e);
         } // end of try-catch
         c = null;
      } // end of if ()
   }
   
   public void ejbLoad()
   {
      id = (Integer) ctx.getPrimaryKey();
      if (id == null) 
      {
         Category.getInstance(getClass().getName()).info("null id!");
      } // end of if ()
      
      PreparedStatement ps = null;
      try 
      {
      
         ps = getConnection().prepareStatement("SELECT BALANCE, CUSTOMERID FROM CCBMPACCOUNT WHERE ID = ?");
         if (ps == null) 
         {
            Category.getInstance(getClass().getName()).info("WFT? null ps!");
         } // end of if ()
         
         ps.setInt(1, id.intValue());
         ResultSet rs = ps.executeQuery();
         if (rs.next() == false)
            throw new NoSuchEntityException("Account does not exist " + id.toString());
         this.balance = rs.getInt(1);
         this.customerId = (Integer)rs.getObject(2);
      }
      catch (Exception e)
      {
         Category.getInstance(getClass().getName()).info("Exception in ejbLoad", e);
         throw new EJBException(e);
      } // end of try-catch
      finally
      {
         try 
         {
            if (ps != null) {ps.close();}
         }
         catch (Exception e)
         {
            Category.getInstance(getClass().getName()).info("Exception closing ps: " + e);
         } // end of try-catch
      } // end of finally
   }
   
   public void ejbStore()
   {
      PreparedStatement ps = null;
      try 
      {
         ps = getConnection().prepareStatement("UPDATE CCBMPACCOUNT SET BALANCE = ?, CUSTOMERID = ? WHERE ID = ?");         
         ps.setInt(1, balance);
         ps.setObject(2, customerId);
         ps.setInt(3, id.intValue());
         ps.execute();
      }
      catch (Exception e)
      {
         Category.getInstance(getClass().getName()).info("Exception in ejbStore", e);
         throw new EJBException(e);
      } // end of try-catch
      finally
      {
         try 
         {
            if (ps != null) {ps.close();}
         }
         catch (Exception e)
         {
            Category.getInstance(getClass().getName()).info("Exception closing ps: " + e);
         } // end of try-catch
      } // end of finally
      
   }
   
   public void ejbRemove()
   {
      PreparedStatement ps = null;
      try 
      {
         ps = getConnection().prepareStatement("DELETE FROM CCBMPACCOUNT WHERE ID = ?");         
         ps.setInt(1, id.intValue());
         ps.execute();
      }
      catch (Exception e)
      {
         Category.getInstance(getClass().getName()).info("Exception in ejbRemove", e);
         throw new EJBException(e);
      } // end of try-catch
      finally
      {
         try 
         {
            if (ps != null) {ps.close();}
         }
         catch (Exception e)
         {
            Category.getInstance(getClass().getName()).info("Exception closing ps: " + e);
         } // end of try-catch
      } // end of finally
   }
   
   public void setEntityContext(EntityContext ctx)
   {
      this.ctx = ctx;
   }
   
   public void unsetEntityContext()
   {
      ctx = null;
   }

   private Connection getConnection() throws Exception
   {
      if (c == null) 
      {
         DataSource ds = (DataSource)new InitialContext().lookup("java:/DefaultDS");
         c = ds.getConnection();
         
      } // end of if ()
      return c;
   }
}

