
package org.jboss.test;

import java.net.URL;
import java.security.AccessController;
import java.security.CodeSource;
import java.security.Permission;
import java.security.PermissionCollection;
import java.security.PrivilegedAction;
import java.security.ProtectionDomain;
import java.util.PropertyPermission;
import javax.security.auth.Policy;
import javax.security.auth.Subject;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;

import org.jboss.security.SimplePrincipal;
import org.jboss.security.SecurityPolicy;
import org.jboss.security.SecurityPolicyParser;

/** Tests of the SecurityPolicyParser and SecurityPolicy classes.

@author Scott.Stark@jboss.org
@version $Revision: 1.3 $
*/
public class TestSecurityPolicyParser extends junit.framework.TestCase
{

    public TestSecurityPolicyParser(String name)
    {
	super(name);
    }

    protected void setUp() throws Exception
    {
	String policyName = "tst-policy.xml";
	URL policyURL = getClass().getClassLoader().getResource(policyName);
	SecurityPolicyParser policyStore = new SecurityPolicyParser(policyURL);
	SecurityPolicy policy = new SecurityPolicy(policyStore);
	Policy.setPolicy(policy);
	policy.refresh();
	Configuration.setConfiguration(policy.getLoginConfiguration());
    }

    public void testParser()
    {
	Subject subject = new Subject();
	subject.getPrincipals().add(new SimplePrincipal("starksm"));
	CodeSource cs = new CodeSource(null, null);
	Policy policy = Policy.getPolicy();
	SecurityPolicy.setActiveApp("test-domain");
	PermissionCollection perms = policy.getPermissions(subject, cs);
	Permission p = new NamespacePermission("Project1/Documents/Public", "r---");
	boolean implied = perms.implies(p);
	assertTrue(p.toString(), implied);
    }

    public void testSubject() throws LoginException
    {
        LoginContext lc = new LoginContext("test-domain");
        lc.login();
        Subject subject = lc.getSubject();
        System.out.println("Subject="+subject);
        SecurityPolicy.setActiveApp("test-domain");
        CodeSource cs = new CodeSource(null, null);
        ProtectionDomain pd = getClass().getProtectionDomain();
        cs = pd.getCodeSource();
        Subject.doAsPrivileged(subject, new PrivilegedAction()
            {
                public Object run()
                {
                    SecurityDelegate.accessProject("Project1", "r---", false);
                    SecurityDelegate.accessProject("Project1", "rw--", false);
                    SecurityDelegate.accessProject("Project1", "rw-d", false);
                    SecurityDelegate.accessProject("Project1/Documents/Private", "rwxd", false);
                    SecurityDelegate.accessProject("Project1/Documents/Public", "r---", true);
                    SecurityDelegate.accessProject("Project1/Documents/Public/readme.html", "r---", true);
                    SecurityDelegate.accessProject("Project1/Documents/Public/readme.html", "rw--", false);
                    return null;
                }
            },
            null
        );
    }

    /** The code that executes the custom permission check needs to be in a
        separate class so that we can jar it up to create a codesource whcich
	is disticnt from the TestSecurityPolicyParser codesource.
    */
    static class SecurityDelegate
    {
        static void accessProject(String path, String action, boolean shouldAllow)
        {
            Permission p = new NamespacePermission(path, action);
            try
            {
                AccessController.checkPermission(p);
                if( shouldAllow == false )
                   throw new IllegalStateException("Access allowed for: "+path+", action="+action);
	    }
	    catch(SecurityException e)
	    {
                if( shouldAllow == true )
                    throw new IllegalStateException("Access denied for: "+path+", action="+action);
	    }

	    try
	    {
	        // Try a Java2 permission check that should fail for everyone
                p = new PropertyPermission("java.class.path", "write");
                AccessController.checkPermission(p);
                throw new IllegalStateException("access allowed for: "+p);
            }
            catch(SecurityException e)
            {
            }
        }
    }

    public static void main(String[] args) throws Exception
    {
	TestSecurityPolicyParser tst = new TestSecurityPolicyParser("main");
	tst.setUp();
	//tst.testParser();
	tst.testSubject();
    }
}
