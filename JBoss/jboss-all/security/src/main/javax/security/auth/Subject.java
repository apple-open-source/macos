/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.security.auth;

import java.io.Serializable;
import java.security.AccessControlContext;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.security.Permission;
import java.security.Principal;
import java.util.AbstractSet;
import java.util.Iterator;
import java.util.HashSet;
import java.util.Set;

/** An alternate implementation of the JAAS 1.0 Configuration class that deals
 * with ClassLoader shortcomings that were fixed in the JAAS included with
 * JDK1.4 and latter. This version allows LoginModules to be loaded from the
 * Thread context ClassLoader and uses an XML based configuration by default.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1 $
 */
public final class Subject implements Serializable
{
   private static final long serialVersionUID = -8308522755600156056L;
   private static AuthPermission SET_READ_ONLY_PERM = new AuthPermission("setReadOnly");
   private static AuthPermission MOD_PRINCIPALS_PERM = new AuthPermission("modifyPrincipals");
   private static AuthPermission MOD_PUBLIC_CREDS_PERM = new AuthPermission("modifyPublicCredentials");
   private static AuthPermission MOD_PRIVATE_CREDS_PERM = new AuthPermission("modifyPrivateCredentials");

   private boolean readOnly;
   private Set principals;
   private Set publicCredentials;
   private Set privateCredentials;

   public static Subject getSubject(AccessControlContext acc)
   {
      throw new UnsupportedOperationException("getSubject is not supported by"
         + " this version of JAAS 1.0, use the JDK 1.4 version");
   }
   public static Object doAs(Subject subject, PrivilegedAction action)
   {
      throw new UnsupportedOperationException("doAs is not supported by this"
         + " version of JAAS 1.0, use the JDK 1.4 version");
   }
   public static Object doAs(Subject subject, PrivilegedExceptionAction action)
      throws PrivilegedActionException
   {
      throw new UnsupportedOperationException("doAs is not supported by this"
         + " version of JAAS 1.0, use the JDK 1.4 version");
   }
   public static Object doAsPrivileged(Subject subject, PrivilegedAction action,
      AccessControlContext acc)
   {
      throw new UnsupportedOperationException("doAsPrivileged is not supported"
         + " by this version of JAAS 1.0, use the JDK 1.4 version");

   }
   public static Object doAsPrivileged(Subject subject, PrivilegedExceptionAction action,
      AccessControlContext acc)
      throws PrivilegedActionException
   {
      throw new UnsupportedOperationException("doAsPrivileged is not supported"
         + " by this version of JAAS 1.0, use the JDK 1.4 version");
   }

   public Subject()
   {
      this(false, null, null, null);
   }

   public Subject(boolean readOnly, Set principals,
      Set pubCredentials, Set privCredentials)
   {
      this.readOnly = readOnly;
      this.principals = new ProtectedSet(this, principals, MOD_PRINCIPALS_PERM);
      this.publicCredentials = new ProtectedSet(this, pubCredentials, MOD_PUBLIC_CREDS_PERM);
      this.privateCredentials = new ProtectedSet(this, privCredentials, MOD_PRIVATE_CREDS_PERM);
   }

   public boolean equals(Object obj)
   {
      if( obj == null )
          return false;
      if( obj == this )
          return true;
      if( (obj instanceof Subject) == false )
         return false;

      Subject subj = (Subject) obj;
      boolean equals = getPrincipals().equals(subj.getPrincipals());
      if( equals )
         equals = getPublicCredentials().equals(subj.getPublicCredentials());
      if( equals )
         equals = getPrivateCredentials().equals(subj.getPrivateCredentials());
      return equals;
   }

   public int hashCode()
   {
      int hashCode = 0;
      Iterator iter = getPrincipals().iterator();
      while( iter.hasNext() )
      {
         Object next = iter.next();
         hashCode ^= next.hashCode();
      }
      iter = getPublicCredentials().iterator();
      while( iter.hasNext() )
      {
         Object next = iter.next();
         hashCode ^= next.hashCode();
      }
      iter = getPrivateCredentials().iterator();
      while( iter.hasNext() )
      {
         Object next = iter.next();
         hashCode ^= next.hashCode();
      }
      return hashCode;
   }

   public boolean isReadOnly()
   {
      return readOnly;
   }
   public void setReadOnly()
   {
      SecurityManager sm = System.getSecurityManager();
      if( sm != null )
      {
         sm.checkPermission(SET_READ_ONLY_PERM);
      }
      readOnly = true;
   }

   public Set getPrincipals()
   {
      return principals;
   }
   public Set getPrincipals(Class c)
   {
      Set copy = new ProtectedSet(this, principals, MOD_PRINCIPALS_PERM, c);
      return copy;
   }

   public Set getPublicCredentials()
   {
      return publicCredentials;
   }
   public Set getPublicCredentials(Class c)
   {
      Set copy = new ProtectedSet(this, publicCredentials, MOD_PUBLIC_CREDS_PERM, c);
      return copy;
   }
   public Set getPrivateCredentials()
   {
      return privateCredentials;
   }
   public Set getPrivateCredentials(Class c)
   {
      Set copy = new ProtectedSet(this, privateCredentials, MOD_PRIVATE_CREDS_PERM, c);
      return copy;
   }

   public String toString()
   {
      StringBuffer tmp = new StringBuffer("Subject(");
      tmp.append("Principals{");
      Iterator iter = principals.iterator();
      while( iter.hasNext() )
      {
         Object next = iter.next();
         tmp.append(next);
         if( iter.hasNext() )
            tmp.append(',');
      }
      tmp.append('}');

      tmp.append("PublicCredentials{");
      iter = publicCredentials.iterator();
      while( iter.hasNext() )
      {
         Object next = iter.next();
         tmp.append(next);
         if( iter.hasNext() )
            tmp.append(',');
      }
      tmp.append('}');

      tmp.append("PrivateCredentials{count=");
      tmp.append(privateCredentials.size());
      tmp.append('}');

      tmp.append(')');
      return tmp.toString();
   }

   private static class ProtectedSet extends AbstractSet implements Serializable
   {
      private HashSet content;
      private Permission modifyPerm;
      private Subject subject;

      private ProtectedSet(Subject subject, Set s, Permission modifyPerm)
      {
         this(subject, s, modifyPerm, null);
      }
      private ProtectedSet(Subject subject, Set s, Permission modifyPerm, Class filterClass)
      {
         this.subject = subject;
         this.modifyPerm = modifyPerm;
         if( s != null )
         {
            if( filterClass != null )
            {
               this.content = new HashSet();
               Iterator iter = s.iterator();
               while( iter.hasNext() )
               {
                  Object next = iter.next();
                  if( filterClass.isInstance(next) )
                     this.content.add(next);
               }
            }
            else
            {
               this.content = new HashSet(s);
            }
         }
         else
         {
            this.content = new HashSet(3);
         }
      }

      public synchronized boolean add(Object obj)
      {
         if( subject.isReadOnly() == true )
            throw new IllegalStateException("Subject is read-only");
         SecurityManager sm = System.getSecurityManager();
         if( sm != null )
         {
            sm.checkPermission(modifyPerm);
         }
         // Only Principals may be added to the principals set
         if( modifyPerm == MOD_PRINCIPALS_PERM )
         {
            if( (obj instanceof Principal) == false )
               throw new IllegalArgumentException("Only Principals may be added to the PrinpalsSet");
         }
         return content.add(obj);
      }

      public synchronized boolean remove(Object obj)
      {
         if( subject.isReadOnly() == true )
            throw new IllegalStateException("Subject is read-only");
         SecurityManager sm = System.getSecurityManager();
         if( sm != null )
         {
            sm.checkPermission(modifyPerm);
         }
         return content.remove(obj);
      }

      public Iterator iterator()
      {
         final Iterator iterator = content.iterator();
         return new Iterator()
         {
            public boolean hasNext()
            {
               return iterator.hasNext();
            }

            public Object next()
            {
               Object next = iterator.next();
               // If this is the private credentials set check permissions
               SecurityManager sm = System.getSecurityManager();
               if( sm != null && modifyPerm == MOD_PRIVATE_CREDS_PERM )
               {
                  String name = next.getClass().getName() + " * *";
                  PrivateCredentialPermission pcp = new PrivateCredentialPermission(name, "read");
                  sm.checkPermission(pcp);
               }

               return next;
            }

            public void remove()
            {
               SecurityManager sm = System.getSecurityManager();
               if( sm != null )
               {
                  sm.checkPermission(modifyPerm);
               }
               iterator.remove();
            }
         };
      }

      public int size()
      {
         return content.size();
      }
   }
}
