package org.jboss.test;

import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.security.SimplePrincipal;

/** Tests of propagating the security identity across threads using
InheritableThreadLocal.

@author Scott.Stark@jboss.org
@version $Revision: 1.1 $
*/
public class ThreadLocalTestCase extends TestCase
{
    private static InheritableThreadLocal thread_principal = new InheritableThreadLocal();
    private static InheritableThreadLocal thread_credential = new InheritableThreadLocal();
    private static String USER = "jduke";
    private static String PASSWORD = "theduke";

    public ThreadLocalTestCase(String name)
    {
        super(name);
    }

    public void testSecurityPropagation() throws Exception
    {
        // Assign the principal & crendentials for this thread
        SimplePrincipal user = new SimplePrincipal(USER);
        thread_principal.set(user);
        thread_credential.set(PASSWORD);
        // Spawn a thread 
        Thread t = new Thread(new Child(), "testSecurityPropagation");
        t.start();
        t.join();
    }

    public void testSecurityPropagation2() throws Exception
    {
        // Assign the principal & crendentials for this thread
        SimplePrincipal user = new SimplePrincipal(USER);
        thread_principal.set(user);
        thread_credential.set(PASSWORD);
        // Spawn a thread 
        Thread t = new Thread(new Child(), "testSecurityPropagation");
        // See that changing the current thread info is not seen by children threads
        thread_principal.set(new SimplePrincipal("other"));
        thread_credential.set("otherpass");
        t.start();
        t.join();
    }

    static class Child implements Runnable
    {
        public void run()
        {
            Thread t = Thread.currentThread();
            System.out.println("Child.run begin, t="+t);
            if( t.getName().equals("testSecurityPropagation") )
            {
                SimplePrincipal user = (SimplePrincipal) thread_principal.get();
                String password = (String) thread_credential.get();
                if( user.getName().equals(USER) == false )
                    fail("Thread user != "+USER);
                if( password.equals(PASSWORD) == false )
                    fail("Thread password != "+PASSWORD);
            }
            System.out.println("Child.run end, t="+t);
        }
    }

    public static void main(java.lang.String[] args)
    {
        System.setErr(System.out);
        TestSuite suite = new TestSuite(ThreadLocalTestCase.class);
        junit.textui.TestRunner.run(suite);
    }
    
}
