
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.ejb;

import java.rmi.RemoteException;
import java.sql.Connection;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;
import javax.naming.InitialContext;
import javax.sql.DataSource;

import org.jboss.test.jca.interfaces.Reentrant;


/**
 * ReentrantBean.java tests if CachedConnectionManager works with reentrant ejbs.
 *
 *
 * Created: Wed Jul 31 13:27:44 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
    * @ejb.bean
    *    jndi-name="ejb/jca/Reentrant"
    *    name="Reentrant"
    *    type="BMP"
    *    view-type="remote"
    *    reentrant="true"
    * @ejb.pk class="java.lang.Integer"
    * @ejb.transaction
    *    type="Required"
 *
 */

public class ReentrantBean
   implements EntityBean 
{

   private Integer id;

   private EntityContext ctx;

   public ReentrantBean (){
      
   }


   /**
    * Creates a new <code>ejbCreate</code> instance.
    *
    * @param id an <code>Integer</code> value
    * @param other a <code>Reentrant</code> value
    * @exception CreateException if an error occurs
    * @exception RemoteException if an error occurs
    *
    * @ejb.create-method 
    */
   public Integer ejbCreate(Integer id, Reentrant other) throws CreateException, RemoteException
   {
      this.id = id;
      return id;
   }

   /**
    * Creates a new <code>ejbPostCreate</code> instance.
    *
    * @param id an <code>Integer</code> value
    * @param other a <code>Reentrant</code> value
    * @exception CreateException if an error occurs
    * @exception RemoteException if an error occurs
    */
   public void ejbPostCreate(Integer id, Reentrant other) throws CreateException, RemoteException
   {
      this.id = id;
      Reentrant me = (Reentrant)ctx.getEJBObject();
      Connection c = null;
      try
      {
	 try
	 {
	    DataSource ds = (DataSource)new InitialContext().lookup("java:/DefaultDS");
	    c = ds.getConnection();
	    if (other != null)
	    {
	       other.doSomething(me);
	    }
	 }
	 finally
	 {
	    c.close();
	 }
      }
      catch (Exception e)
      {
	 throw new CreateException("could not get DataSource or Connection" + e.getMessage());
      }
   }

   /**
    * Describe <code>doSomething</code> method here.
    *
    * @param first a <code>Reentrant</code> value
    * @exception RemoteException if an error occurs
    *
    * @ejb.interface-method 
    */
   public void doSomething(Reentrant first) throws RemoteException
   {
      if (first != null)
      {
	 first.doSomething(null);
      }
   }

   /**
    * Describe <code>findByPrimaryKey</code> method here.
    *
    * @param id an <code>Integer</code> value
    * @return an <code>Integer</code> value
    */
   public Integer ejbFindByPrimaryKey(Integer id)
   {
      return id;
   }


   // implementation of javax.ejb.EntityBean interface

   /**
    *
    * @exception javax.ejb.EJBException <description>
    * @exception java.rmi.RemoteException <description>
    */
   public void ejbActivate()
   {
      // TODO: implement this javax.ejb.EntityBean method
   }

   /**
    *
    * @exception javax.ejb.EJBException <description>
    * @exception java.rmi.RemoteException <description>
    */
   public void ejbLoad()
   {
      this.id = (Integer)ctx.getPrimaryKey();
   }

   /**
    *
    * @exception javax.ejb.EJBException <description>
    * @exception java.rmi.RemoteException <description>
    */
   public void ejbPassivate()
   {
      // TODO: implement this javax.ejb.EntityBean method
   }

   /**
    *
    * @exception javax.ejb.RemoveException <description>
    * @exception javax.ejb.EJBException <description>
    * @exception java.rmi.RemoteException <description>
    */
   public void ejbRemove() throws EJBException
   {
      // TODO: implement this javax.ejb.EntityBean method
   }

   /**
    *
    * @exception javax.ejb.EJBException <description>
    * @exception java.rmi.RemoteException <description>
    */
   public void ejbStore() throws EJBException
   {
      // TODO: implement this javax.ejb.EntityBean method
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.ejb.EJBException <description>
    * @exception java.rmi.RemoteException <description>
    */
   public void setEntityContext(EntityContext ctx)
   {
      this.ctx = ctx;
   }

   /**
    *
    * @exception javax.ejb.EJBException <description>
    * @exception java.rmi.RemoteException <description>
    */
   public void unsetEntityContext()
   {
      ctx = null;
   }

}
