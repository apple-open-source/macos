/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.audit.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/**
 * Session facade for audit testing.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.2 $
 */
public interface AuditSession
   extends EJBObject
{
   public void createAudit(String id)
      throws RemoteException;
   public void updateAudit(String id, String stringValue)
      throws RemoteException;

   public String fullAuditCheck(String id, String user, long beginTime, long endTime)
      throws RemoteException;
   public String createAuditCheck(String id, String user, long beginTime, long endTime)
      throws RemoteException;
   public String updateAuditCheck(String id, String user, long beginTime, long endTime)
      throws RemoteException;

   public void createAuditChangedNames(String id)
      throws RemoteException;
   public void updateAuditChangedNames(String id, String stringValue)
      throws RemoteException;
   public String createAuditChangedNamesCheck(String id, String user, long beginTime, long endTime)
      throws RemoteException;
   public String updateAuditChangedNamesCheck(String id, String user, long beginTime, long endTime)
      throws RemoteException;

   public void createAuditMapped(String id)
      throws RemoteException;
   public void updateAuditMapped(String id, String stringValue)
      throws RemoteException;
   public void createAuditMappedChangedFields(String id, String user, long time)
      throws RemoteException;
   public void updateAuditMappedChangedFields(String id, String stringValue, String user, long time)
      throws RemoteException;
   public String createAuditMappedCheck(String id, String user, long beginTime, long endTime)
      throws RemoteException;
   public String updateAuditMappedCheck(String id, String user, long beginTime, long endTime)
      throws RemoteException;

}
