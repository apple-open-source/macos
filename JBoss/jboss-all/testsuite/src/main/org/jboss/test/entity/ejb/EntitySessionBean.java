/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.entity.ejb;

import java.rmi.RemoteException;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;

import org.jboss.test.entity.interfaces.EntitySession;
import org.jboss.test.entity.interfaces.EntitySessionHome;
import org.jboss.test.entity.interfaces.Pathological;
import org.jboss.test.entity.interfaces.PathologicalEntity;
import org.jboss.test.entity.interfaces.PathologicalEntityHome;

/**
 * Session facade for entity testing.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.1 $
 */
public class EntitySessionBean
   implements SessionBean
{
   private transient SessionContext ctx;

   public void createPathological(String name, boolean pathological)
   {
      Pathological.setPathological(pathological);
      try
      {
         PathologicalEntityHome home = getPathologicalEJB();
         home.create(name);
      }
      catch (Throwable e)
      {
         check(e);
      }
      finally
      {
         Pathological.setPathological(false);
      }
   }

   public void removeHomePathological(String name, boolean pathological)
   {
      Pathological.setPathological(pathological);
      try
      {
         PathologicalEntityHome home = getPathologicalEJB();
         home.remove(name);
      }
      catch (Throwable e)
      {
         check(e);
      }
      finally
      {
         Pathological.setPathological(false);
      }
   }

   public void removePathological(String name, boolean pathological)
   {
      try
      {
         PathologicalEntityHome home = getPathologicalEJB();
         PathologicalEntity bean = home.findByPrimaryKey(name);
         Pathological.setPathological(pathological);
         bean.remove();
      }
      catch (Throwable e)
      {
         check(e);
      }
      finally
      {
         Pathological.setPathological(false);
      }
   }

   public void findPathological(String name, boolean pathological)
   {
      Pathological.setPathological(pathological);
      try
      {
         PathologicalEntityHome home = getPathologicalEJB();
         home.findByPrimaryKey(name);
      }
      catch (Throwable e)
      {
         check(e);
      }
      finally
      {
         Pathological.setPathological(false);
      }
   }

   public void getPathological(String name, boolean pathological)
   {
      try
      {
         PathologicalEntityHome home = getPathologicalEJB();
         PathologicalEntity bean = home.findByPrimaryKey(name);
         Pathological.setPathological(pathological);
         bean.getSomething();
      }
      catch (Throwable e)
      {
         check(e);
      }
      finally
      {
         Pathological.setPathological(false);
      }
   }

   public void setPathological(String name, boolean pathological)
   {
      try
      {
         PathologicalEntityHome home = getPathologicalEJB();
         PathologicalEntity bean = home.findByPrimaryKey(name);
         Pathological.setPathological(pathological);
         bean.setSomething("something");
      }
      catch (Throwable e)
      {
         check(e);
      }
      finally
      {
         Pathological.setPathological(false);
      }
   }

   public void ejbCreate()
      throws CreateException
   {
   }
   
   public void setSessionContext(SessionContext ctx) 
   {
      this.ctx = ctx;
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

   private void check(Throwable e)
   {
      while (true)
      {
         if (e instanceof EJBException)
            e = ((EJBException) e).getCausedByException();
         else if (e instanceof RemoteException)
            e = ((RemoteException) e).detail;
         else if (e instanceof IllegalStateException)
            throw (IllegalStateException) e;
         else
            return;
      }
   }

   private PathologicalEntityHome getPathologicalEJB()
      throws Exception
   {
      return (PathologicalEntityHome) new InitialContext().lookup("java:comp/env/ejb/PathologicalEJB");
   }
}
