/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jmx.interceptors;

import java.security.Principal;
import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.Map;
import javax.naming.InitialContext;

import org.jboss.mx.interceptor.AbstractInterceptor;
import org.jboss.mx.interceptor.Invocation;
import org.jboss.mx.interceptor.InvocationException;
import org.jboss.logging.Logger;
import org.jboss.security.RealmMapping;
import org.jboss.security.SubjectSecurityManager;
import org.jboss.security.SimplePrincipal;
import org.jboss.invocation.MarshalledInvocation;

/** A role based security interceptor that requries the caller of
 * any write operations to have a JNDIWriter role and the caller of any
 * read operations to have a JNDIReader role.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public final class JNDISecurity
   extends AbstractInterceptor
{
   private static Logger log = Logger.getLogger(JNDISecurity.class);
   private static final Principal READER_ROLE = new SimplePrincipal("JNDIReader");
   private static final Principal WRITER_ROLE = new SimplePrincipal("JNDIWriter");

   private String securityDomain;
   private SubjectSecurityManager authMgr;
   private RealmMapping roleMgr;
   private Map methodMap;

   public String getSecurityDomain()
   {
      return securityDomain;
   }
   public void setSecurityDomain(String securityDomain) throws Exception
   {
      log.info("setSecurityDomain: "+securityDomain);
      this.securityDomain = securityDomain;
      InitialContext ctx = new InitialContext();
      this.authMgr = (SubjectSecurityManager) ctx.lookup(securityDomain);
      this.roleMgr = (RealmMapping) ctx.lookup(securityDomain);
   }

   // Interceptor overrides -----------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      String opName = invocation.getName();
      log.info("invoke, opName="+opName);

      // If this is not the invoke(Invocation) op just pass it along
      if( opName.equals("invoke") == false )
         return getNext().invoke(invocation);

      Object[] args = invocation.getArgs();
      org.jboss.invocation.Invocation invokeInfo =
         (org.jboss.invocation.Invocation) args[0];
      // There must be a valid security manager
      if( authMgr == null || roleMgr == null )
      {
         String msg = "No security mgr configured, check securityDomain: "+securityDomain;
         SecurityException se = new SecurityException(msg);
         throw new InvocationException(se, "Invalid securityDomain");
      }

      // Get the security context passed from the client
      Principal principal = invokeInfo.getPrincipal();
      Object credential = invokeInfo.getCredential();
      if( authMgr.isValid(principal, credential) == false )
      {
         String msg = "Failed to authenticate principal: "+principal;
         SecurityException se = new SecurityException(msg);
         throw new InvocationException(se, "Authentication failure");
      }

      // See what operation is being attempted
      if( methodMap == null )
         initMethodMap(invocation);
      HashSet methodRoles = new HashSet();
      if( invokeInfo instanceof MarshalledInvocation )
      {
         MarshalledInvocation mi = (MarshalledInvocation) invokeInfo;
         mi.setMethodMap(methodMap);
      }
      Method method = invokeInfo.getMethod();
      boolean isRead = isReadMethod(method);
      if( isRead == true )
         methodRoles.add(READER_ROLE);
      else
         methodRoles.add(WRITER_ROLE);
      if( roleMgr.doesUserHaveRole(principal, methodRoles) == false )
      {
         String msg = "Failed to authorize subject: "+authMgr.getActiveSubject()
            + " principal: " + principal
            + " for access roles:" + methodRoles;
         SecurityException se = new SecurityException(msg);
         throw new InvocationException(se, "Authorization failure");
      }

      // Let the invocation go
      return getNext().invoke(invocation);
   }

   private boolean isReadMethod(Method method)
   {
      boolean isRead = true;
      String name = method.getName();
      isRead = name.equals("lookup") || name.equals("list")
         || name.equals("listBindings");
      return isRead;
   }

   /**
    * 
    */ 
   private void initMethodMap(Invocation invocation) throws InvocationException
   {
      Invocation getMethodMap = new Invocation("MethodMap",
         Invocation.ATTRIBUTE, Invocation.READ, null, null,
         invocation.getDescriptors(), invocation.getResource());
      methodMap = (Map) getNext().invoke(getMethodMap);
   }
}
