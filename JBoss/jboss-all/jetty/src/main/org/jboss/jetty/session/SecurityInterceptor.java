
package org.jboss.jetty.session;

//----------------------------------------

import java.security.Principal;
import javax.servlet.http.HttpSession;
import org.apache.log4j.Category;
import org.jboss.security.SecurityAssociation;
import org.mortbay.j2ee.session.Manager;
import org.mortbay.j2ee.session.AroundInterceptor;
import org.mortbay.j2ee.session.State;

//----------------------------------------

// accesses proprietary JBoss classes (JBossSX)

// We need to ensure that calls to the HttpSession implementation are
// made in Jetty's and not the User's Security Context. Then we can
// have a Jetty/WebContainer user/role and secure use of the
// HttpSession EJBs to this user/role...

// this would be better implemented as a dynamic proxy - but...

// we use thread local temp storage since the user may have multiple
// threads with different Principal/Credentials running through us
// concurrently...

public class SecurityInterceptor
  extends AroundInterceptor
{
  Category _log=Category.getInstance(getClass().getName());

  protected final Principal _ourPrincipal=null;
  protected final Object    _ourCredential=null;

  protected ThreadLocal     _theirPrincipal=new ThreadLocal();
  protected ThreadLocal     _theirCredential=new ThreadLocal();

  protected void
    before()
  {
    //    _log.info("pushing security context");

    _theirPrincipal.set(SecurityAssociation.getPrincipal());
    _theirCredential.set(SecurityAssociation.getCredential());

    SecurityAssociation.setPrincipal(_ourPrincipal);
    SecurityAssociation.setCredential(_ourCredential);
  }

  protected void
    after()
  {
    //    _log.info("popping security context");

    SecurityAssociation.setPrincipal((Principal)_theirPrincipal.get());
    SecurityAssociation.setCredential(_theirCredential.get());

    _theirPrincipal.set(null);
    _theirCredential.set(null);
  }
}
