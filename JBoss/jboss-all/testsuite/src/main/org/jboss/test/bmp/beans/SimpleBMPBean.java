
package org.jboss.test.bmp.beans;

import java.rmi.RemoteException;
import java.util.Vector;
import java.util.Collection;

import java.sql.*;

import javax.naming.*;
import javax.ejb.*;
import javax.sql.DataSource;

public class SimpleBMPBean
implements EntityBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   EntityContext ctx = null;
   DataSource ds = null;

   // bmp fields
   Integer id;
   String name;
   
   public Integer ejbCreate (int _id, String _name)
      throws CreateException, RemoteException
   {
      log.debug ("ejbCreate (int, String) called");
      
      id = new Integer (_id);
      
      boolean dublicate = false;
      
      Connection con = null;
      try
      {
         con = ds.getConnection ();
         Statement s = con.createStatement ();
         ResultSet rs = s.executeQuery ("SELECT id FROM simplebean WHERE id=" + id.toString ());
         dublicate = rs.next ();
         rs.close ();
         s.close ();
         
         if (!dublicate)
         {
            PreparedStatement ps = con.prepareStatement ("INSERT INTO simplebean VALUES (?,?)");
            ps.setInt (1, _id);
            ps.setString (2, _name);
            ps.execute ();
            ps.close ();

            name = _name;
         }
      }
      catch (Exception _e)
      {
         throw new EJBException ("couldnt create: "+_e.getMessage ());
      }
      finally 
      {
         try
         {
            if (con != null)
               con.close ();
         }
         catch (Exception _sqle)
         {
         }
      }
      
      if (dublicate)
         throw new DuplicateKeyException ("Bean with id="+_id+" already exists.");

      return id;      
   }
   
   public Integer ejbCreateMETHOD (int _id, String _name)
      throws CreateException, RemoteException
   {
      log.debug ("ejbCreateMETHOD (int, String) called");
      
      id = new Integer (_id);
      
      boolean dublicate = false;
      
      Connection con = null;
      try
      {
         con = ds.getConnection ();
         Statement s = con.createStatement ();
         ResultSet rs = s.executeQuery ("SELECT id FROM simplebean WHERE id=" + id.toString ());
         dublicate = rs.next ();
         rs.close ();
         s.close ();
         
         if (!dublicate)
         {
            PreparedStatement ps = con.prepareStatement ("INSERT INTO simplebean VALUES (?,?)");
            ps.setInt (1, _id);
            ps.setString (2, _name);
            ps.execute ();
            ps.close ();

            name = _name;
         }
      }
      catch (Exception _e)
      {
         throw new EJBException ("couldnt create: "+_e.getMessage ());
      }
      finally 
      {
         try
         {
            if (con != null)
               con.close ();
         }
         catch (Exception _sqle)
         {
         }
      }
      
      if (dublicate)
         throw new DuplicateKeyException ("Bean with id="+_id+" already exists.");

      return id;      
   }
   
   public void ejbPostCreate (int _id, String _name)
      throws CreateException, RemoteException
   {
      log.debug ("ejbPostCreate (int, String) called");
   }
   
   public void ejbPostCreateMETHOD (int _id, String _name)
      throws CreateException, RemoteException
   {
      log.debug ("ejbPostCreateMETHOD (int, String) called");
   }
   
   public void ejbLoad ()
   {
      log.debug ("ejbLoad () called");

      
      Connection con = null;
      try
      {
         con = ds.getConnection ();
         PreparedStatement ps = con.prepareStatement ("SELECT id,name FROM simplebean WHERE id=?");
         ps.setInt (1, ((Integer)ctx.getPrimaryKey ()).intValue ());
         ResultSet rs = ps.executeQuery ();
         if (rs.next ())
         {
            id = new Integer (rs.getInt ("id"));
            name = rs.getString ("name");            
         }
         rs.close ();
         ps.close ();
      }
      catch (Exception _e)
      {
         throw new EJBException ("couldnt load: "+_e.getMessage ());
      }
      finally 
      {
         try
         {
            if (con != null)
               con.close ();
         }
         catch (Exception _sqle)
         {
         }
      }
   }
   
   public void ejbStore ()
   {
      log.debug ("ejbStore () called");

      Connection con = null;
      try
      {
         con = ds.getConnection ();
         PreparedStatement ps = con.prepareStatement ("UPDATE simplebean SET name=? WHERE id=?");
         ps.setString (1, name);
         ps.setInt (2, id.intValue ());
         ps.execute ();
         ps.close ();
      }
      catch (Exception _e)
      {
         throw new EJBException ("couldnt store: "+_e.getMessage ());
      }
      finally 
      {
         try
         {
            if (con != null)
               con.close ();
         }
         catch (Exception _sqle)
         {
         }
      }
   }
   
   public void ejbRemove ()
   {
      log.debug ("ejbRemove () called");

      Connection con = null;
      try
      {
         con = ds.getConnection ();
         PreparedStatement ps = con.prepareStatement ("DELETE FROM simplebean WHERE id=?");
         ps.setInt (1, id.intValue ());
         ps.execute ();
         ps.close ();
      }
      catch (Exception _e)
      {
         throw new EJBException ("couldnt remove: "+_e.getMessage ());
      }
      finally 
      {
         try
         {
            if (con != null)
               con.close ();
         }
         catch (Exception _sqle)
         {
         }
      }
   }

   
   public Integer ejbFindByPrimaryKey (Integer _key) throws FinderException
   {
      log.debug ("ejbFindByPrimaryKey (Integer) called");

      Connection con = null;
      boolean found = false;
      try
      {
         con = ds.getConnection ();
         PreparedStatement ps = con.prepareStatement ("SELECT id FROM simplebean WHERE id=?");
         ps.setInt (1, _key.intValue ());
         ResultSet rs = ps.executeQuery ();
         found = rs.next ();
         rs.close ();
         ps.close ();
      }
      catch (Exception _e)
      {
         throw new EJBException ("couldnt seek: "+_e.getMessage ()); 
      }
      finally 
      {   
         
         try
         {
            if (con != null)
               con.close ();
         }
         catch (Exception _sqle)
         {
         }
      }
      if (!found)
         throw new FinderException ("No bean with id="+_key+" found.");
      
      return _key;
   }

   public Collection ejbFindAll () throws FinderException
   {
      log.debug ("ejbFindAll () called");

      Connection con = null;
      Vector result = new Vector ();
      try
      {
         con = ds.getConnection ();
         Statement s = con.createStatement ();
         ResultSet rs = s.executeQuery ("SELECT id FROM simplebean");
         while (rs.next ())
         {
            result.add (new Integer (rs.getInt ("id")));
         }
         rs.close ();
         s.close ();
      }
      catch (Exception _e)
      {
         throw new EJBException ("couldnt seek: "+_e.getMessage ()); 
      }
      finally 
      {   
         
         try
         {
            if (con != null)
               con.close ();
         }
         catch (Exception _sqle)
         {
         }
      }
      
      return result;
   }


   
   public void ejbActivate ()
   {
      log.debug ("ejbActivate () called");
   }
   
   public void ejbPassivate ()
   {
      log.debug ("ejbPassivate () called");
   }
   
   public void setEntityContext (EntityContext _ctx)
   {
      log.debug ("setEntityContext (\""+_ctx.getPrimaryKey ()+"\") called");

      ctx = _ctx;
      // lookup the datasource
      try
      {
         ds = (DataSource)new InitialContext ().lookup ("java:comp/env/datasource");
      }
      catch (NamingException _ne)
      {
         throw new EJBException ("Datasource not found: "+_ne.getMessage ());
      }
   }
   
   public void unsetEntityContext ()
   {
      log.debug ("unsetEntityContext () called");

      ctx = null;
   }
   
   // business methods ---------------------------------------------------------------
   
   public void setName (String _name)
   {
      name = _name;
   }
   
   public String getName ()
   {
      return name;
   }
   
   
}
