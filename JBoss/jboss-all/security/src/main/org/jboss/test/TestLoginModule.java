package org.jboss.test;

import java.util.Map;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.LoginException;
import javax.security.auth.spi.LoginModule;
import org.jboss.security.SimplePrincipal;

public class TestLoginModule implements LoginModule
{
   Subject subject;
   String principal;
   String name;
   boolean succeed;
   boolean throwEx;

   public TestLoginModule()
   {
   }

   public void initialize(Subject subject, CallbackHandler handler, Map sharedState, Map options)
   {
      this.subject = subject;
      principal = (String) options.get("principal");
      if( principal == null )
          principal = "guest";
      name = (String) options.get("name");
      String opt = (String) options.get("succeed");
      succeed = Boolean.valueOf(opt).booleanValue();
      opt = (String) options.get("throwEx");
      throwEx = Boolean.valueOf(opt).booleanValue();
      System.out.println("initialize, name="+name);
      opt = (String) options.get("initEx");
      if( Boolean.valueOf(opt) == Boolean.TRUE )
         throw new IllegalArgumentException("Failed during init, name="+name);
   }

   public boolean login() throws LoginException
   {
      System.out.println("login, name="+name+", succeed="+succeed);
      if( throwEx )
         throw new LoginException("Failed during login, name="+name);
      return succeed;
   }

   public boolean commit() throws LoginException
   {
      System.out.println("commit, name="+name);
      subject.getPrincipals().add(new SimplePrincipal(principal));
      subject.getPublicCredentials().add("A public credential");
      subject.getPrivateCredentials().add("A private credential");
      return true;
   }

   public boolean abort() throws LoginException
   {
      System.out.println("abort, name="+name);
      return true;
   }

   public boolean logout() throws LoginException
   {
      System.out.println("logout, name="+name);
      subject.getPrincipals().remove(new SimplePrincipal(principal));
      return succeed;
   }

}
