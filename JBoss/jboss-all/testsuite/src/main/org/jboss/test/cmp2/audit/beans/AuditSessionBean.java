/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.audit.beans;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.ResultSetMetaData;
import java.sql.Timestamp;
import java.sql.Types;
import java.util.Date;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import javax.security.auth.login.LoginContext;
import javax.sql.DataSource;

import org.jboss.test.cmp2.audit.interfaces.ApplicationCallbackHandler;
import org.jboss.test.cmp2.audit.interfaces.Audit;
import org.jboss.test.cmp2.audit.interfaces.AuditHome;
import org.jboss.test.cmp2.audit.interfaces.AuditMapped;
import org.jboss.test.cmp2.audit.interfaces.AuditMappedHome;

/**
 * Session facade for audit testing.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.2 $
 */
public class AuditSessionBean
   implements SessionBean
{
   private static final int FULL = 1;
   private static final int CREATE = 2;
   private static final int UPDATE = 3;
   private static final int CREATE_CHANGED_NAMES = 4;
   private static final int UPDATE_CHANGED_NAMES = 5;
   private static final int CREATE_MAPPED = 6;
   private static final int UPDATE_MAPPED = 7;

   private static String QUERY_FULL = "select audit_created_by, audit_created_time, audit_updated_by, audit_updated_time"
                                      + " from cmp2_audit where id = ?";
   private static String QUERY_CREATE = "select audit_created_by, audit_created_time"
                                        + " from cmp2_audit where id = ?";
   private static String QUERY_UPDATE = "select audit_updated_by, audit_updated_time"
                                        + " from cmp2_audit where id = ?";
   private static String QUERY_CREATE_CHANGED_NAMES = "select createdby, createdtime"
                                                      + " from cmp2_audit_changednames where id = ?";
   private static String QUERY_UPDATE_CHANGED_NAMES = "select updatedby, updatedtime"
                                                      + " from cmp2_audit_changednames where id = ?";
   private static String QUERY_CREATE_MAPPED = "select createdby, createdtime"
                                               + " from cmp2_audit_mapped where id = ?";
   private static String QUERY_UPDATE_MAPPED = "select updatedby, updatedtime"
                                               + " from cmp2_audit_mapped where id = ?";

   public void createAudit(String id)
   {
      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser1", "user1");
         try
         {
            AuditHome home = getAuditEJB();
            home.create(id);
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public void updateAudit(String id, String stringValue)
   {
      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser2", "user2");
         try
         {
            AuditHome home = getAuditEJB();
            Audit audit = home.findByPrimaryKey(id);
            audit.setStringValue(stringValue);
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public void createAuditChangedNames(String id)
   {
      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser1", "user1");
         try
         {
            AuditHome home = getAuditChangedNamesEJB();
            home.create(id);
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public void updateAuditChangedNames(String id, String stringValue)
   {
      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser2", "user2");
         try
         {
            AuditHome home = getAuditChangedNamesEJB();
            Audit audit = home.findByPrimaryKey(id);
            audit.setStringValue(stringValue);
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public void createAuditMapped(String id)
   {
      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser1", "user1");
         try
         {
            AuditMappedHome home = getAuditMappedEJB();
            home.create(id);
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public void updateAuditMapped(String id, String stringValue)
   {
      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser2", "user2");
         try
         {
            AuditMappedHome home = getAuditMappedEJB();
            AuditMapped audit = home.findByPrimaryKey(id);
            audit.setStringValue(stringValue);
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public void createAuditMappedChangedFields(String id, String user, long time)
   {
      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser1", "user1");
         try
         {
            AuditMappedHome home = getAuditMappedEJB();
            AuditMapped audit = home.create(id);
            audit.setCreatedBy(user);
            audit.setCreatedTime(new Date(time));
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public void updateAuditMappedChangedFields(String id, String stringValue, String user, long time)
   {
      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser2", "user2");
         try
         {
            AuditMappedHome home = getAuditMappedEJB();
            AuditMapped audit = home.findByPrimaryKey(id);
            audit.setStringValue(stringValue);
            audit.setUpdatedBy(user);
            audit.setUpdatedTime(new Date(time));
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public String fullAuditCheck(String id, String user, long beginTime, long endTime)
   {
      try
      {
         AuditData auditData = getAuditData(id, FULL);
         if (user.equals(auditData.createdBy) == false)
            return "Expected created by to be set to " + user + " during the test but got " + auditData.createdBy;
         if (auditData.createdTime < beginTime || auditData.createdTime > endTime)
            return "Expected created time to be set between " + 
                   beginTime + "-" + endTime + " during the test but got " + auditData.createdTime;
         if (user.equals(auditData.updatedBy) == false)
            return "Expected updated by to be set to " + user + " during the test but got " + auditData.updatedBy;
         if (auditData.updatedTime < beginTime || auditData.updatedTime > endTime)
            return "Expected updated time to be set between " + 
                   beginTime + "-" + endTime + " during the test but got " + auditData.updatedTime;
         return null;
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public String createAuditCheck(String id, String user, long beginTime, long endTime)
   {
      return createCheck(id, CREATE, user, beginTime, endTime);
   }

   public String updateAuditCheck(String id, String user, long beginTime, long endTime)
   {
      return updateCheck(id, UPDATE, user, beginTime, endTime);
   }

   public String createAuditChangedNamesCheck(String id, String user, long beginTime, long endTime)
   {
      return createCheck(id, CREATE_CHANGED_NAMES, user, beginTime, endTime);
   }

   public String updateAuditChangedNamesCheck(String id, String user, long beginTime, long endTime)
   {
      return updateCheck(id, UPDATE_CHANGED_NAMES, user, beginTime, endTime);
   }

   public String createAuditMappedCheck(String id, String user, long beginTime, long endTime)
   {
      String failure = createCheck(id, CREATE_MAPPED, user, beginTime, endTime);
      if (failure != null)
         return failure;

      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser1", "user1");
         try
         {
            AuditMappedHome home = getAuditMappedEJB();
            AuditMapped audit = home.findByPrimaryKey(id);
            if (user.equals(audit.getCreatedBy()) == false)
               return "Expected getter to return user from test";
            long time = audit.getCreatedTime().getTime();
            if (time < beginTime || time > endTime)
               return "Expected getter to return time from test";

            return null;
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public String updateAuditMappedCheck(String id, String user, long beginTime, long endTime)
   {
      String failure = updateCheck(id, UPDATE_MAPPED, user, beginTime, endTime);
      if (failure != null)
         return failure;

      try
      {
         LoginContext login = ApplicationCallbackHandler.login("audituser1", "user1");
         try
         {
            AuditMappedHome home = getAuditMappedEJB();
            AuditMapped audit = home.findByPrimaryKey(id);
            if (user.equals(audit.getUpdatedBy()) == false)
               return "Expected getter to return user from test";
            long time = audit.getUpdatedTime().getTime();
            if (time < beginTime || time > endTime)
               return "Expected getter to return time from test";

            return null;
         }
         finally
         {
            if (login != null)
               login.logout();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public String createCheck(String id, int type, String user, long beginTime, long endTime)
   {
      try
      {
         AuditData auditData = getAuditData(id, type);
         if (user.equals(auditData.createdBy) == false)
            return "Expected created by to be set to " + user + " during the test but got " + auditData.createdBy;
         if (auditData.createdTime < beginTime || auditData.createdTime > endTime)
            return "Expected created time to be set between " + 
                   beginTime + "-" + endTime + " during the test but got " + auditData.createdTime;
         return null;
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public String updateCheck(String id, int type, String user, long beginTime, long endTime)
   {
      try
      {
         AuditData auditData = getAuditData(id, type);
         if (user.equals(auditData.updatedBy) == false)
            return "Expected updated by to be set to " + user + " during the test but got " + auditData.updatedBy;
         if (auditData.updatedTime < beginTime || auditData.updatedTime > endTime)
            return "Expected updated time to be set between " + 
                   beginTime + "-" + endTime + " during the test but got " + auditData.updatedTime;
         return null;
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }

   public void ejbCreate()
      throws CreateException
   {
   }
   
   public void setSessionContext(SessionContext ctx) 
   {
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

   private AuditData getAuditData(String id, int type)
      throws Exception
   {
      Connection c = getDataSource().getConnection();
      PreparedStatement s = null;
      try
      {
         switch (type)
         {
         case FULL:
            s = c.prepareStatement(QUERY_FULL);
            break;
         case CREATE:
            s = c.prepareStatement(QUERY_CREATE);
            break;
         case UPDATE:
            s = c.prepareStatement(QUERY_UPDATE);
            break;
         case CREATE_CHANGED_NAMES:
            s = c.prepareStatement(QUERY_CREATE_CHANGED_NAMES);
            break;
         case UPDATE_CHANGED_NAMES:
            s = c.prepareStatement(QUERY_UPDATE_CHANGED_NAMES);
            break;
         case CREATE_MAPPED:
            s = c.prepareStatement(QUERY_CREATE_MAPPED);
            break;
         case UPDATE_MAPPED:
            s = c.prepareStatement(QUERY_UPDATE_MAPPED);
            break;
//         case CREATE_UNSECURED:
//            s = c.prepareStatement(QUERY_CREATE_UNSECURED);
//            break;
//         case UPDATE_UNSECURED:
//            s = c.prepareStatement(QUERY_UPDATE_UNSECURED);
//            break;
         }
         s.setString(1, id);
         ResultSet r = s.executeQuery();
         r.next();

         switch (type)
         {
         case FULL:
            return new AuditData(r.getString(1), getTimestamp(r, 2),
                                 r.getString(3), getTimestamp(r, 4));
         case CREATE:
         case CREATE_CHANGED_NAMES:
         case CREATE_MAPPED:
            return new AuditData(r.getString(1), getTimestamp(r, 2), null, 0);
         case UPDATE:
         case UPDATE_CHANGED_NAMES:
         case UPDATE_MAPPED:
            return new AuditData(null, 0, r.getString(1), getTimestamp(r, 2));
         }

         return null;
      }
      finally
      {
         if (s != null)
            s.close();
         if (c != null)
            c.close();
      }
   }

   private AuditHome getAuditEJB()
      throws Exception
   {
      return (AuditHome) new InitialContext().lookup("java:comp/env/ejb/AuditEJB");
   }

   private AuditHome getAuditChangedNamesEJB()
      throws Exception
   {
      return (AuditHome) new InitialContext().lookup("java:comp/env/ejb/AuditChangedNamesEJB");
   }

   private AuditMappedHome getAuditMappedEJB()
      throws Exception
   {
      return (AuditMappedHome) new InitialContext().lookup("java:comp/env/ejb/AuditMappedEJB");
   }

   private DataSource getDataSource()
      throws Exception
   {
      return (DataSource) new InitialContext().lookup("java:comp/env/jdbc/DataSource");
   }

   private static long getTimestamp(ResultSet r, int index)
      throws Exception
   {
      ResultSetMetaData metaData = r.getMetaData();
      switch (metaData.getColumnType(index))
      {
      case Types.DATE:
         return r.getDate(index).getTime();
      case Types.TIMESTAMP:
         Timestamp timestamp = r.getTimestamp(index);
         long time = timestamp.getTime();
         if (time % 1000 == 0)
            time += timestamp.getNanos() / 1000000;
         return time;
      }
      return -1;
   }

   public static class AuditData
   {
      public String createdBy;
      public long createdTime;
      public String updatedBy;
      public long updatedTime;

      public AuditData(String createdBy, long createdTime, String updatedBy, long updatedTime)
      {
         this.createdBy = createdBy;
         this.createdTime = createdTime;
         this.updatedBy = updatedBy;
         this.updatedTime = updatedTime;
      }
   }
}
