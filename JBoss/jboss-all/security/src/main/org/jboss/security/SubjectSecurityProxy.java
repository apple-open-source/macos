/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.UndeclaredThrowableException;
import java.lang.reflect.Method;
import java.security.PrivilegedExceptionAction;
import java.security.PrivilegedActionException;
import javax.security.auth.Subject;


import org.jboss.security.SecurityPolicy;
import org.jboss.security.SubjectSecurityManager;

/** A subclass of AbstractSecurityProxy that executes as the currently
 authenticated subject within the invokeHomeOnDelegate and invokeOnDelegate
 methods. The current subject is accessed via the security manager passed
 to the init() method, which must be an instance of SubjectSecurityManager.
 This results in AccessController.checkPermission() calls made from within the
 security delegate methods to be based on the Subject's permissions.

 This is just an experiment with the JAAS Subject based permissions.

 @see javax.security.auth.Policy
 @see javax.security.auth.Subject
 @see org.jboss.security.SecurityPolicy
 @see org.jboss.security.SubjectSecurityManager

 @author Scott.Stark@jboss.org
 @version $Revision: 1.3.4.3 $
 */
public class SubjectSecurityProxy extends AbstractSecurityProxy
{
   private SubjectSecurityManager subjectSecurityManager;

   SubjectSecurityProxy(Object delegate)
   {
      super(delegate);
   }

   public void init(Class beanHome, Class beanRemote, Object securityMgr)
      throws InstantiationException
   {
      init(beanHome, beanRemote, null, null, securityMgr);
   }

   public void init(Class beanHome, Class beanRemote,
      Class beanLocalHome, Class beanLocal, Object securityMgr)
      throws InstantiationException
   {
      if ((securityMgr instanceof SubjectSecurityManager) == false)
      {
         String msg = "SubjectSecurityProxy requires a SubjectSecurityManager"
            + " instance, securityMgr=" + securityMgr;
         throw new InstantiationException(msg);
      }
      subjectSecurityManager = (SubjectSecurityManager) securityMgr;
      super.init(beanHome, beanRemote, beanLocalHome, beanLocal, securityMgr);
   }

   protected void invokeHomeOnDelegate(final Method m, final Object[] args, final Object delegate)
      throws SecurityException
   {   // Get authenticated subject and invoke invokeAsSubject in Subject.doAsPrivaledged() block...
      final Subject subject = subjectSecurityManager.getActiveSubject();
      if (subject == null)
         throw new SecurityException("No subject associated with secure proxy");

      try
      {
         String domainName = subjectSecurityManager.getSecurityDomain();
         SecurityPolicy.setActiveApp(domainName);
         Subject.doAsPrivileged(subject, new PrivilegedExceptionAction()
         {
            public Object run() throws Exception
            {
               m.invoke(delegate, args);
               return null;
            }
         },
            null
         );
      }
      catch (PrivilegedActionException e)
      {
         Throwable t = e.getException();
         if (t instanceof InvocationTargetException)
         {
            t = ((InvocationTargetException) t).getTargetException();
         }
         else if (t instanceof UndeclaredThrowableException)
         {
            t = ((UndeclaredThrowableException) t).getUndeclaredThrowable();
         }
         if (t instanceof SecurityException)
            throw (SecurityException) t;
         t.printStackTrace();
         throw new SecurityException("Unexpected error during security proxy execution:" + t.getMessage());
      }
   }

   protected void invokeOnDelegate(final Method m, final Object[] args, final Object delegate)
      throws Exception
   {   // Get authenticated subject and invoke invokeAsSubject in Subject.doAsPrivaledged() block...
      final Subject subject = subjectSecurityManager.getActiveSubject();
      if (subject == null)
         throw new SecurityException("No subject associated with secure proxy");

      try
      {
         String domainName = subjectSecurityManager.getSecurityDomain();
         SecurityPolicy.setActiveApp(domainName);
         Subject.doAsPrivileged(subject, new PrivilegedExceptionAction()
         {
            public Object run() throws Exception
            {
               m.invoke(delegate, args);
               return null;
            }
         },
            null
         );
      }
      catch (PrivilegedActionException e)
      {
         Throwable t = e.getException();
         if (t instanceof InvocationTargetException)
         {
            // This is a declared exception, just throw it
            InvocationTargetException ex = (InvocationTargetException) t;
            t = ex.getTargetException();
            throw (Exception) t;
         }
         else if (t instanceof UndeclaredThrowableException)
         {
            t = ((UndeclaredThrowableException) t).getUndeclaredThrowable();
         }
         if (t instanceof SecurityException)
            throw (SecurityException) t;
         throw new SecurityException("Unexpected error during security proxy execution:" + t.getMessage());
      }
   }

}
