/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jca.ejb;

import java.sql.Connection;
import java.sql.SQLException;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import javax.sql.DataSource;

/**
 * A stateful session bean that has an unshareable resource
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 */
public class UnshareableConnectionStatefulBean
   implements SessionBean
{
   private SessionContext ctx;

   Connection c;

   public void runTestPart1()
   {
      try
      {
         if (c.getAutoCommit())
            throw new EJBException("Autocommit should be off");
      }
      catch (SQLException e)
      {
         throw new EJBException(e.toString());
      }
   }

   public void runTestPart2()
   {
      try
      {
         c.commit();
      }
      catch (SQLException e)
      {
         throw new EJBException(e.toString());
      }
   }

   public void ejbCreate()
      throws CreateException
   {
      initConnection();
   }

   public void ejbActivate()
   {
      initConnection();
   }

   public void ejbPassivate()
   {
      termConnection();
   }

   public void ejbRemove()
   {
      termConnection();
   }

   public void initConnection()
   {
      if (c != null)
         throw new EJBException("Connection already inited");

      try
      {
         DataSource ds = (DataSource) new InitialContext().lookup("java:comp/env/jdbc/DataSource");
         c = ds.getConnection();
         c.setAutoCommit(false);
      }
      catch (Exception e)
      {
         throw new EJBException(e.toString());
      }
   }

   public void termConnection()
   {
      if (c == null)
         throw new EJBException("Connection already terminated");

      try
      {
         c.close();
         c = null;
      }
      catch (Exception e)
      {
         throw new EJBException(e.toString());
      }
   }

   public void setSessionContext(SessionContext ctx)
   {
      this.ctx = ctx;
   }

   public void unsetSessionContext()
   {
   }
}

