/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security;

import java.security.Principal;
import java.util.ArrayList;
import javax.security.auth.Subject;

/** The SecurityAssociation class maintains the security principal and
credentials. This can be done on either a singleton basis or a thread
local basis depending on the server property. When the server property has
been set to true, the security information is maintained in thread local
storage. The type of thread local storage depends on the
org.jboss.security.SecurityAssociation.ThreadLocal property.
If this property is true, then the thread local storage object is of
type java.lang.ThreadLocal which results in the current thread's
security information NOT being propagated to child threads.

When the property is false or does not exist, the thread local storage object
is of type java.lang.InheritableThreadLocal, and any threads spawned by the
current thread will inherit the security information of the current thread.
Subseqent changes to the current thread's security information are NOT
propagated to any previously spawned child threads.

When the server property is false, security information is maintained in
class variables which makes the information available to all threads within
the current VM.

@author Daniel O'Connor (docodan@nycap.rr.com)
@author Scott.Stark@jboss.org
@version $Revision: 1.10.4.1 $
 */
public final class SecurityAssociation
{
   /** A flag indicating if security information is global or thread local */
   private static boolean server;
   /** The SecurityAssociation principal used when the server flag is false */
   private static Principal principal;
   /** The SecurityAssociation credential used when the server flag is false */
   private static Object credential;
   /** The SecurityAssociation Subject used when the server flag is false */
   private static Subject subject;

   /** The SecurityAssociation principal used when the server flag is true */
   private static ThreadLocal threadPrincipal;
   /** The SecurityAssociation credential used when the server flag is true */
   private static ThreadLocal threadCredential;
   /** The SecurityAssociation Subject used when the server flag is true */
   private static ThreadLocal threadSubject;

   /** Thread local stacks of run-as principal roles used to implement J2EE
    run-as identity propagation */
   private static RunAsThreadLocalStack threadRunAsStacks = new RunAsThreadLocalStack();
   /** The permission required to access getPrincpal, getCredential, getSubject */
   private static final RuntimePermission getPrincipalInfoPermission =
      new RuntimePermission("org.jboss.security.SecurityAssociation.getPrincipalInfo");
   /** The permission required to access setPrincpal, setCredential, setSubject */
   private static final RuntimePermission setPrincipalInfoPermission =
      new RuntimePermission("org.jboss.security.SecurityAssociation.setPrincipalInfo");
   /** The permission required to access setServer */
   private static final RuntimePermission setServerPermission =
      new RuntimePermission("org.jboss.security.SecurityAssociation.setServer");
   /** The permission required to access pushRunAsRole/popRunAsRole */
   private static final RuntimePermission setRunAsRole =
      new RuntimePermission("org.jboss.security.SecurityAssociation.setRunAsRole");

   static
   {
      boolean useThreadLocal = false;
      try
      {
         useThreadLocal = Boolean.getBoolean("org.jboss.security.SecurityAssociation.ThreadLocal");
      }
      catch(SecurityException e)
      {
         // Ignore and use the default
      }
      
      if( useThreadLocal )
      {
         threadPrincipal = new ThreadLocal();
         threadCredential = new ThreadLocal();
         threadSubject = new ThreadLocal();
      }
      else
      {
         threadPrincipal = new InheritableThreadLocal();
         threadCredential = new InheritableThreadLocal();
         threadSubject = new InheritableThreadLocal();
      }
   }

   /** Get the current principal information.
    If a security manager is present, then this method calls the security
    manager's <code>checkPermission</code> method with a
    <code>
    RuntimePermission("org.jboss.security.SecurityAssociation.getPrincipalInfo")
    </code>
    permission to ensure it's ok to access principal information.
    If not, a <code>SecurityException</code> will be thrown.
    @return Principal, the current principal identity.
    */
   public static Principal getPrincipal()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(getPrincipalInfoPermission);

      if (server)
         return (Principal) threadPrincipal.get();
      else
         return principal;
   }
   
   /** Get the current principal credential information. This can be of
    any type including: a String password, a char[] password, an X509 cert,
    etc.
    If a security manager is present, then this method calls the security
    manager's <code>checkPermission</code> method with a
    <code>
    RuntimePermission("org.jboss.security.SecurityAssociation.getPrincipalInfo")
    </code>
    permission to ensure it's ok to access principal information.
    If not, a <code>SecurityException</code> will be thrown.
    @return Object, the credential that proves the principal identity.
    */
   public static Object getCredential()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(getPrincipalInfoPermission);

      if (server)
         return threadCredential.get();
      else
         return credential;
   }

   /** Get the current Subject information.
    If a security manager is present, then this method calls the security
    manager's <code>checkPermission</code> method with a
    <code>
    RuntimePermission("org.jboss.security.SecurityAssociation.getPrincipalInfo")
    </code>
    permission to ensure it's ok to access principal information.
    If not, a <code>SecurityException</code> will be thrown.
    @return Subject, the current Subject identity.
    */
   public static Subject getSubject()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(getPrincipalInfoPermission);

      if (server)
         return (Subject) threadSubject.get();
      else
         return subject;
   }

   /** Set the current principal information.
    If a security manager is present, then this method calls the security
    manager's <code>checkPermission</code> method with a
    <code>
    RuntimePermission("org.jboss.security.SecurityAssociation.setPrincipalInfo")
    </code>
    permission to ensure it's ok to access principal information.
    If not, a <code>SecurityException</code> will be thrown.
    @param principal, the current principal identity.
    */
   public static void setPrincipal( Principal principal )
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(setPrincipalInfoPermission);

      if (server)
         threadPrincipal.set( principal );
      else
         SecurityAssociation.principal = principal;
   }
   
   /** Set the current principal credential information. This can be of
    any type including: a String password, a char[] password, an X509 cert,
    etc.

    If a security manager is present, then this method calls the security
    manager's <code>checkPermission</code> method with a
    <code>
    RuntimePermission("org.jboss.security.SecurityAssociation.setPrincipalInfo")
    </code>
    permission to ensure it's ok to access principal information.
    If not, a <code>SecurityException</code> will be thrown.
    @param credential, the credential that proves the principal identity.
    */
   public static void setCredential( Object credential )
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(setPrincipalInfoPermission);

      if (server)
         threadCredential.set( credential );
      else
         SecurityAssociation.credential = credential;
   }

   /** Set the current Subject information.
    If a security manager is present, then this method calls the security
    manager's <code>checkPermission</code> method with a
    <code>
    RuntimePermission("org.jboss.security.SecurityAssociation.setPrincipalInfo")
    </code>
    permission to ensure it's ok to access principal information.
    If not, a <code>SecurityException</code> will be thrown.
    @param principal, the current principal identity.
    */
   public static void setSubject(Subject subject)
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(setPrincipalInfoPermission);

      if (server)
         threadSubject.set( subject );
      else
         SecurityAssociation.subject = subject;
   }

   /** Clear all principal information.
    If a security manager is present, then this method calls the security
    manager's <code>checkPermission</code> method with a
    <code>
    RuntimePermission("org.jboss.security.SecurityAssociation.setPrincipalInfo")
    </code>
    permission to ensure it's ok to access principal information.
    If not, a <code>SecurityException</code> will be thrown.
    @param principal, the current principal identity.
    */
   public static void clear()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(setPrincipalInfoPermission);
      if( server == true )
      {
         threadPrincipal.set(null);
         threadCredential.set(null);
         threadSubject.set(null);
      }
      else
      {
         SecurityAssociation.principal = null;
         SecurityAssociation.credential = null;
         SecurityAssociation.subject = null;
      }
   }

   /** Push the current thread of control's run-as principal role.
    */
   public static void pushRunAsRole(Principal runAsRole)
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(setRunAsRole);
      threadRunAsStacks.push(runAsRole);
   }
   /** Pop the current thread of control's run-as principal role.
    */
   public static Principal popRunAsRole()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(setRunAsRole);
      Principal runAsRole = threadRunAsStacks.pop();
      return runAsRole;
   }
   /** Look at the current thread of control's run-as principal role.
    */
   public static Principal peekRunAsRole()
   {
      Principal runAsRole = threadRunAsStacks.peek();
      return runAsRole;
   }

   /** Set the server mode of operation. When the server property has
    been set to true, the security information is maintained in thread local
    storage. This should be called to enable property security semantics
    in any multi-threaded environment where more than one thread requires
    that security information be restricted to the thread's flow of control.

    If a security manager is present, then this method calls the security
    manager's <code>checkPermission</code> method with a
    <code>
    RuntimePermission("org.jboss.security.SecurityAssociation.setServer")
    </code>
    permission to ensure it's ok to access principal information.
    If not, a <code>SecurityException</code> will be thrown.
    */
   public static void setServer()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
         sm.checkPermission(setPrincipalInfoPermission);

      server = true;
   }

   /** A subclass of ThreadLocal that implements a value stack
    using an ArrayList and implements push, pop and peek stack
    operations on the thread local ArrayList.
    */
   private static class RunAsThreadLocalStack extends ThreadLocal
   {
      protected Object initialValue()
      {
         return new ArrayList();
      }
      void push(Principal runAs)
      {
         ArrayList stack = (ArrayList) super.get();
         stack.add(runAs);
      }
      Principal pop()
      {
         ArrayList stack = (ArrayList) super.get();
         Principal runAs = null;
         int lastIndex = stack.size() - 1;
         if( lastIndex >= 0 )
            runAs = (Principal) stack.remove(lastIndex);
         return runAs;
      }
      Principal peek()
      {
         ArrayList stack = (ArrayList) super.get();
         Principal runAs = null;
         int lastIndex = stack.size() - 1;
         if( lastIndex >= 0 )
            runAs = (Principal) stack.get(lastIndex);
         return runAs;
      }
   }
}
