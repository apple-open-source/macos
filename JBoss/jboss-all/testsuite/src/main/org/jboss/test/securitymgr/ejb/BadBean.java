package org.jboss.test.securitymgr.ejb;

import java.security.Principal;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.apache.log4j.Category;

import org.jboss.security.SecurityAssociation;

/** A session bean that attempts things that should not be allowed
when running JBoss with a security manager.
 
@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
 */
public class BadBean implements SessionBean
{
   static final Category log = Category.getInstance(BadBean.class);

   private SessionContext sessionContext;

   public void ejbCreate()
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

   public void setSessionContext(SessionContext context)
   {
      sessionContext = context;
   }

   /** Creates a new instance of BadBean */
   public BadBean()
   {
   }
   
   public void accessSystemProperties()
   {
      System.getProperty("java.home");
      System.setProperty("java.home","tjo");
   }
   
   public Principal getSecurityAssociationPrincipal()
   {
      return SecurityAssociation.getPrincipal();
   }
   public Object getSecurityAssociationCredential()
   {
      return SecurityAssociation.getCredential();
   }
   public void setSecurityAssociationPrincipal(Principal user)
   {
      SecurityAssociation.setPrincipal(user);
   }
   public void setSecurityAssociationCredential(char[] password)
   {
      SecurityAssociation.setCredential(password);
   }
}
