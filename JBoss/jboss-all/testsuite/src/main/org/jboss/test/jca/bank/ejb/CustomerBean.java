
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jca.bank.ejb;


import java.util.Collection;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import javax.sql.DataSource;
import java.sql.SQLException;

import javax.ejb.ObjectNotFoundException;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.EntityContext;
import javax.ejb.FinderException;
import javax.ejb.NoSuchEntityException;
import javax.naming.InitialContext;
import java.util.ArrayList;

import org.apache.log4j.Category;

import org.jboss.test.jca.bank.interfaces.AccountLocal;
import org.jboss.test.jca.bank.interfaces.AccountLocalHome;


/**
 * Describe class <code>CustomerBean</code> here.
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version 1.0
 * @ejb:bean   name="Customer"
 *             local-jndi-name="LocalCustomer"
 *             view-type="local"
 *             type="BMP"
 *             primkey-field="id"
 * @ejb:pk class="java.lang.Integer"
 */
public class CustomerBean
   implements EntityBean
{

   private Connection c;

   public Integer id;
   public String name;
   public Collection accounts;

   private EntityContext ctx;   
   
   /**
    * field get-set pair for field id
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
    * @ejb:interface-method view-type="local"
    */
   public void setId(final Integer id)
   {
      this.id = id;
   }
   
   
   /**
    * field get-set pair for field name
    * Get the value of name
    * @return value of name
    *
    * @ejb:interface-method
    */
   public String getName()
   {
      return name;
   }
   
   /**
    * Set the value of name
    * @param name  Value to assign to name
    *
    * @ejb:interface-method view-type="local"
    */
   public void setName(final String name)
   {
      this.name = name;
   }
   
   
   
   /**
    * field get-set pair for field accounts
    * Get the value of accounts
    * @return value of accounts
    *
    * @ejb:interface-method
    */
   public Collection getAccounts()
   {
      return accounts;
   }
   
   

   
   /**
    * Describe <code>addAccountLocal</code> method here.
    *
    * @param account an <code>AccountLocal</code> value
    * @ejb:interface-method
    */
   public void addAccount(AccountLocal account)
   {
      try
      {
         account.setCustomerId(id);
         accounts.add(account);
      }
      catch (Exception e)
      {
         throw new EJBException("Problem in addAccount: " + e);
      }
   }
   
   /**
    * Describe <code>removeAccount</code> method here.
    *
    * @param acct an <code>AccountLocal</code> value
    * @ejb:interface-method
    */
   public void removeAccount(AccountLocal account)
   {
      try
      {
         accounts.remove(account);
         account.remove();
      }
      catch (Exception e)
      {
         throw new EJBException("Problem in removeAccount: " + e);
      }

   }
   
   // EntityHome implementation -------------------------------------
   /**
    * Describe <code>ejbCreate</code> method here.
    *
    * @param id an <code>Integer</code> value
    * @param name a <code>String</code> value
    * @return a <code>Integer</code> value
    * @ejb:create-method
    */
   public Integer ejbCreate(Integer id, String name) 
      throws CreateException
   { 
      setId(id);
      setName(name);
      PreparedStatement ps = null;
      try 
      {
      
         ps = getConnection().prepareStatement("INSERT INTO CCBMPCUSTOMER (ID, NAME) VALUES (?, ?)");
         ps.setInt(1, id.intValue());
         ps.setString(2, name);
         accounts = new ArrayList();
      }
      catch (Exception e)
      {
         throw new CreateException("Problem in ejbCreate: " + e);
      } // end of try-catch
      finally
      {
         try 
         {
            ps.close();
         }
         catch (SQLException e)
         {
            Category.getInstance(getClass().getName()).info("SQLException closing ps: " + e);
         } // end of try-catch
      } // end of finally
      return id;
   }

   public void ejbPostCreate(Integer id, String name) 
   { 
   }

   public Integer ejbFindByPrimaryKey(final Integer id)
      throws FinderException
   {
      PreparedStatement ps = null;
      try 
      {
         ps = getConnection().prepareStatement("SELECT ID FROM CCBMPCUSTOMER WHERE ID = ?");
         ps.setInt(1, id.intValue());
         ResultSet rs = ps.executeQuery();
         if (!rs.next()) 
         {
            throw new ObjectNotFoundException("No such customer: " + id);
         } // end of if ()
         rs.close();

      }
      catch (Exception e)
      {
         throw new EJBException("problem in findByPK: " + e);
      } // end of try-catch
      finally
      {
         try 
         {
            ps.close();
         }
         catch (SQLException e)
         {
            Category.getInstance(getClass().getName()).info("SQLException closing ps: " + e);
         } // end of try-catch
      } // end of finally
      return id;
   }
   
   public void ejbPostCreate(String id, String name) 
   { 
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
         catch (SQLException e)
         {
            Category.getInstance(getClass().getName()).info("SQLException closing c: " + e);
         } // end of try-catch
         c = null;
      } // end of if ()
   }
   
   public void ejbLoad()
   {
      id = (Integer) ctx.getPrimaryKey();
      if (id == null)
         throw new EJBException("Null id!");

      PreparedStatement ps = null;
      try 
      {
      
         ps = getConnection().prepareStatement("SELECT NAME FROM CCBMPCUSTOMER WHERE ID = ?");
         ps.setInt(1, id.intValue());
         ResultSet rs = ps.executeQuery();
         if (rs.next() == false)
            throw new NoSuchEntityException("Customer does not exist " + id.toString());
         this.name = rs.getString(1);
         AccountLocalHome ah = (AccountLocalHome)new InitialContext().lookup("AccountLocal");
         accounts = ah.findByCustomerId(id);

      }
      catch (Exception e)
      {
         throw new EJBException("Problem in ejbLoad: " +  e);
      } // end of try-catch
      finally
      {
         try 
         {
            ps.close();
         }
         catch (SQLException e)
         {
            Category.getInstance(getClass().getName()).info("SQLException closing ps: " + e);
         } // end of try-catch
      } // end of finally
   }
   
   public void ejbStore()
   {
      PreparedStatement ps = null;
      try 
      {
         ps = getConnection().prepareStatement("UPDATE CCBMPCUSTOMER SET NAME = ? WHERE ID = ?");         
         ps.setString(1, name);
         ps.setInt(2, id.intValue());
         ps.execute();
      }
      catch (Exception e)
      {
         throw new EJBException("Problem in ejbStore: " + e);
      } // end of try-catch
      finally
      {
         try 
         {
            ps.close();
         }
         catch (SQLException e)
         {
            Category.getInstance(getClass().getName()).info("SQLException closing ps: " + e);
         } // end of try-catch
      } // end of finally
      
   }
   
   public void ejbRemove()
   {
      PreparedStatement ps = null;
      try 
      {
         ps = getConnection().prepareStatement("DELETE FROM CCBMPCUSTOMER WHERE ID = ?");         
         ps.setInt(1, id.intValue());
         ps.execute();
      }
      catch (Exception e)
      {
         throw new EJBException("Problem in ejbRemove: " + e);
      } // end of try-catch
      finally
      {
         try 
         {
            ps.close();
         }
         catch (SQLException e)
         {
            Category.getInstance(getClass().getName()).info("SQLException closing ps: " + e);
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

