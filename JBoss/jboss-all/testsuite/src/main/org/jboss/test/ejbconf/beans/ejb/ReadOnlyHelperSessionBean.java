
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.ejbconf.beans.ejb; // Generated package name

import java.rmi.RemoteException;
import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.Statement;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import javax.sql.DataSource;
/**
 * ReadOnlyHelperSessionBean.java
 *
 *
 * Created: Fri Apr 12 23:37:41 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 *
 * @ejb:bean   name="ReadOnlyHelper"
 *             jndi-name="ReadOnlyHelper"
 *             view-type="remote"
 *             type="Stateless"
 */

public class ReadOnlyHelperSessionBean implements SessionBean  
{
   public ReadOnlyHelperSessionBean ()
   {
      
   }

   /**
    * Describe <code>ejbCreate</code> method here.
    *
    * @ejb:create-method
    */
   public void ejbCreate()
   {
   }

   /**
    * Describe <code>setUp</code> method here.
    *
    * @exception CreateException if an error occurs
    * @ejb:interface-method
    */
   public void setUp()// throws CreateException
   {
      try 
      {
         DataSource ds = (DataSource)new InitialContext().lookup("java:/DefaultDS");
         Connection c = ds.getConnection();
         try 
         {
            Statement s = c.createStatement();
            try 
            {
               s.execute("DELETE FROM READONLY");
               s.execute("INSERT INTO READONLY VALUES (1, 1)");         
            }
            finally
            {
               s.close();
            } // end of try-catch
         }
         finally
         {
            c.close();
         } // end of finally
      }
      catch (Exception e)
      {
	 System.out.println("could not create row for readonly bean");
	 e.printStackTrace();
         //throw new CreateException("could not create row for readonly bean: " + e);
      } // end of try-catch
      
      
   }

   /**
    * Describe <code>checkValue</code> method here.
    *
    * @ejb:interface-method
    */
   public int checkValue()
   {
      try 
      {
         DataSource ds = (DataSource)new InitialContext().lookup("java:/DefaultDS");
         Connection c = ds.getConnection();
         try 
         {
            Statement s = c.createStatement();
            try 
            {
               ResultSet rs = s.executeQuery("SELECT VALUE FROM READONLY WHERE ID=1");
	       try
	       {
		  rs.next();
		  return rs.getInt(1);
	       }
	       finally
	       {
		  rs.close();
	       }
            }
            finally
            {
               s.close();
            } // end of try-catch
         }
         finally
         {
            c.close();
         } // end of finally
      }
      catch (Exception e)
      {
	 System.out.println("could not create row for readonly bean");
	 e.printStackTrace();
	 return -1;
         //throw new CreateException("could not create row for readonly bean: " + e);
      } // end of try-catch
   }
   
   public void ejbActivate()
   {
   }
   
   public void ejbPassivate()
   {
   }
   
   public void ejbRemove()
   {
   }
   
   public void setSessionContext(SessionContext ctx)
   {
   }
   
}// ReadOnlyHelperSessionBean
