/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.audit.test;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.audit.interfaces.AuditSession;
import org.jboss.test.cmp2.audit.interfaces.AuditSessionHome;

/**
 * Tests of audit fields
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.2 $
 */
public class AuditUnitTestCase 
   extends JBossTestCase
{
   public static Test suite() throws Exception
   {
	return JBossTestCase.getDeploySetup(AuditUnitTestCase.class, "cmp2-audit.jar");
   }

   public AuditUnitTestCase(String name)
   {
	super(name);
   }

   public void testCreateAudit()
      throws Exception
   {
      AuditSession audit = getAuditSession();

      long beginTime = System.currentTimeMillis();
      audit.createAudit("createAudit");
      long endTime = System.currentTimeMillis();

      String failure = audit.fullAuditCheck("createAudit", "audituser1", beginTime, endTime);
      if (failure != null)
         fail(failure);
   }

   public void testUpdateAudit()
      throws Exception
   {
      AuditSession audit = getAuditSession();

      long beginCreateTime = System.currentTimeMillis();
      audit.createAudit("updateAudit");
      long endCreateTime = System.currentTimeMillis();
      long beginUpdateTime = System.currentTimeMillis();
      audit.updateAudit("updateAudit", "updateAuditString");
      long endUpdateTime = System.currentTimeMillis();

      String failure = audit.createAuditCheck("updateAudit", "audituser1", beginCreateTime, endCreateTime);
      if (failure != null)
         fail(failure);
      failure = audit.updateAuditCheck("updateAudit", "audituser2", beginUpdateTime, endUpdateTime);
      if (failure != null)
         fail(failure);
   }

   public void testUpdateAuditChangedNames()
      throws Exception
   {
      AuditSession audit = getAuditSession();

      long beginCreateTime = System.currentTimeMillis();
      audit.createAuditChangedNames("updateAudit");
      long endCreateTime = System.currentTimeMillis();
      long beginUpdateTime = System.currentTimeMillis();
      audit.updateAuditChangedNames("updateAudit", "updateAuditString");
      long endUpdateTime = System.currentTimeMillis();

      String failure = audit.createAuditChangedNamesCheck("updateAudit", "audituser1", beginCreateTime, endCreateTime);
      if (failure != null)
         fail(failure);
      failure = audit.updateAuditChangedNamesCheck("updateAudit", "audituser2", beginUpdateTime, endUpdateTime);
      if (failure != null)
         fail(failure);
   }

   public void testUpdateAuditMapped()
      throws Exception
   {
      AuditSession audit = getAuditSession();

      long beginCreateTime = System.currentTimeMillis();
      audit.createAuditMapped("updateAudit");
      long endCreateTime = System.currentTimeMillis();
      long beginUpdateTime = System.currentTimeMillis();
      audit.updateAuditMapped("updateAudit", "updateAuditString");
      long endUpdateTime = System.currentTimeMillis();

      String failure = audit.createAuditMappedCheck("updateAudit", "audituser1", beginCreateTime, endCreateTime);
      if (failure != null)
         fail(failure);
      failure = audit.updateAuditMappedCheck("updateAudit", "audituser2", beginUpdateTime, endUpdateTime);
      if (failure != null)
         fail(failure);
   }

   public void testCreateAuditMappedChangedFields()
      throws Exception
   {
      AuditSession audit = getAuditSession();

      long beginCreateTime = System.currentTimeMillis();
      audit.createAuditMappedChangedFields("createAuditChangedFields", "myUser", 1234);
      long endCreateTime = System.currentTimeMillis();

      String failure = audit.createAuditMappedCheck("createAuditChangedFields", "myUser", 1234, 1234);
      if (failure != null)
         fail(failure);
      failure = audit.updateAuditMappedCheck("createAuditChangedFields", "audituser1", beginCreateTime, endCreateTime);
      if (failure != null)
         fail(failure);
   }

   public void testUpdateAuditMappedChangedFields()
      throws Exception
   {
      AuditSession audit = getAuditSession();

      long beginCreateTime = System.currentTimeMillis();
      audit.createAuditMapped("updateAuditChangedFields");
      long endCreateTime = System.currentTimeMillis();
      audit.updateAuditMappedChangedFields("updateAuditChangedFields", "updateAuditString", "anotherUser", 4567);

      String failure = audit.createAuditMappedCheck("updateAuditChangedFields", "audituser1", beginCreateTime, endCreateTime);
      if (failure != null)
         fail(failure);
      failure = audit.updateAuditMappedCheck("updateAuditChangedFields", "anotherUser", 4567, 4567);
      if (failure != null)
         fail(failure);
   }

   private AuditSession getAuditSession()
   {
      try
      {
         return ((AuditSessionHome) getInitialContext().lookup("cmp2/audit/AuditSession")).create();
      }
      catch(Exception e)
      {
         fail("Exception in getAuditSession: " + e.getMessage());
         return null;
      }
   }
}
