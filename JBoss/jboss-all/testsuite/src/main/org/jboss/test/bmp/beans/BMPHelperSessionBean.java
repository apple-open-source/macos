package org.jboss.test.bmp.beans;


import java.rmi.RemoteException;

import java.sql.*;

import javax.naming.*;
import javax.ejb.*;
import javax.sql.DataSource;

import org.jboss.test.bmp.interfaces.*;



public class BMPHelperSessionBean
implements SessionBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
   SessionContext ctx = null;
   private DataSource ds = null;
   
   public void ejbCreate () throws CreateException, RemoteException
   {
      try
      {
         ds = (DataSource)new InitialContext ().lookup ("java:comp/env/datasource");
      }
      catch (NamingException _ne)
      {
         throw new CreateException ("Datasource not found: "+_ne.getMessage ());
      }
   }
   
   public boolean existsSimpleBeanTable ()
   {
      return tableExists ("SIMPLEBEAN");
   }
   
   public void createSimpleBeanTable ()
   {
      createTable ("CREATE TABLE SIMPLEBEAN (id INTEGER, name VARCHAR(200))");
   }
   
   public void dropSimpleBeanTable ()
   {
      dropTable ("SIMPLEBEAN");
   }
   
   public String doTest () throws RemoteException
   {
      StringBuffer sb = new StringBuffer ();
      SimpleBMP b;
      try
      {
         SimpleBMPHome home = (SimpleBMPHome) new InitialContext ().lookup ("java:comp/env/bean");
         b = home.findByPrimaryKey(new Integer (1));
      }
      catch (Exception _ne)
      {
         throw new EJBException ("couldnt find entity: "+_ne.getMessage ());
      }
      sb.append ("found: "+b.getName ()+"\n");
      sb.append ("set name to \"Name for rollback\"\n");
      b.setName ("Name for rollback");
      sb.append ("current name is: "+b.getName ()+"\n");
      try
      {
         sb.append ("now rolling back...\n");
         
         //marcf FIXME: I don't understand the test below. 
         //
         // ctx.getUserTransaction().rollback ();
         // sb.append ("current name is: "+b.getName ()+"\n");
         //
         //First of all BMP beans are CMT so 
         // you dont' get the "usertransaction" directly, then the proper way to rollback 
         // is to do a 
         // ctx.setRollbackOnly();
         // let the call return, let the call rollback in the transaction interceptor
         // and do a new call to test the value.  Here the sub-call just says that 
         // we are trying to access an instance marked for rollback
         // This test is void in my mind
         // marcf: Fixed with doTestAfterRollback call that should return the right name
         
         ctx.setRollbackOnly();
      }
      catch (Exception _e)
      {
         sb.append ("Error on rolling back: "+_e.getMessage ()+"\n");
      }
      sb.append ("done.");
   
      return sb.toString ();
   }
   
   public String doTestAfterRollback () throws RemoteException
   {
      StringBuffer sb = new StringBuffer ();
      SimpleBMP b;
      try
      {
         SimpleBMPHome home = (SimpleBMPHome) new InitialContext ().lookup ("java:comp/env/bean");
         b = home.findByPrimaryKey(new Integer (1));
      }
      catch (Exception _ne)
      {
         throw new EJBException ("couldnt find entity: "+_ne.getMessage ());
      }
      sb.append ("found: "+b.getName ()+"\n");
      sb.append ("done.");
   
      return sb.toString ();
   }
   
   private boolean tableExists (String _tableName)
   {
      boolean result = false;
      Connection con = null;
      try
      {
         con = ds.getConnection ();
         DatabaseMetaData dmd = con.getMetaData ();
         ResultSet rs = dmd.getTables (con.getCatalog (), null, _tableName, null);
         if (rs.next ())
            result = true;
         
         rs.close ();
      }
      catch (Exception _e)
      {
         throw new EJBException ("Error while looking up table: "+_e.getMessage ());
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
   
   
   private void createTable (String _sql)
   {
      Connection con = null;
      try
      {
         con = ds.getConnection ();
         Statement s = con.createStatement ();
         s.executeUpdate (_sql);
         s.close ();
      }
      catch (Exception _e)
      {
         throw new EJBException ("Error while creating table: "+_e.getMessage ());
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
   
   private void dropTable (String _tableName)
   {
      Connection con = null;
      try
      {
         con = ds.getConnection ();
         Statement s = con.createStatement ();
         s.executeUpdate ("DROP TABLE "+_tableName);
         s.close ();
      }
      catch (Exception _e)
      {
         throw new EJBException ("Error while dropping table: "+_e.getMessage ());
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
   
   
   public void ejbActivate () {}
   public void ejbPassivate () {}
   public void ejbRemove () {}
   public void setSessionContext (SessionContext _ctx) {ctx = _ctx;}
}
