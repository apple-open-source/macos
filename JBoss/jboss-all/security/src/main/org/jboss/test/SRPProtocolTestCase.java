/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test;

import org.apache.log4j.Category;
import org.apache.log4j.NDC;
import org.apache.log4j.PatternLayout;
import org.apache.log4j.WriterAppender;

import org.jboss.logging.XLevel;
import org.jboss.security.Util;
import org.jboss.security.srp.SRPServerInterface;
import org.jboss.security.srp.SRPClientSession;
import org.jboss.security.srp.SRPParameters;

/** Test of the SRP protocol msg exchange sequence.

@author Scott.Stark@jboss.org
@version $Revision: 1.4.2.3 $
*/
public class SRPProtocolTestCase extends junit.framework.TestCase
{
   String username = "stark";
   char[] password = "scott".toCharArray();
   SRPServerInterface server;

   public SRPProtocolTestCase(String name)
   {
       super(name);
   }
   public SRPProtocolTestCase(String name, String username, char[] password)
   {
      super(name);
      this.username = username;
      this.password = password;
   }

   protected void setUp() throws Exception
   {
      // Set up a simple configuration that logs on the console.
       Category root = Category.getRoot();
       root.setLevel(XLevel.TRACE);
       root.addAppender(new WriterAppender(new PatternLayout("%x%m%n"), System.out));
       Util.init();
       NDC.push("S,");
       server = new SimpleSRPServer(password, "123456");
       NDC.pop();
       NDC.remove();
   }

    public void testProtocol() throws Exception
    {
        SRPParameters params = server.getSRPParameters(username);
        NDC.push("C,");
        SRPClientSession client = new SRPClientSession(username, password, params);
        byte[] A = client.exponential();
        NDC.pop();
        NDC.push("S,");
        byte[] B = server.init(username, A);
        NDC.pop();
        NDC.push("C,");
        byte[] M1 = client.response(B);
        NDC.pop();
        NDC.push("S,");
        byte[] M2 = server.verify(username, M1);
        NDC.pop();
        NDC.push("C,");
        if( client.verify(M2) == false )
            throw new SecurityException("Failed to validate server reply");
        NDC.pop();
        NDC.remove();
    }

    /**
    * @param args the command line arguments
    */
    public static void main(String args[])
    {
        long start = System.currentTimeMillis();
        try
        {
            SRPProtocolTestCase tst = null;
            if( args.length == 0 )
               tst = new SRPProtocolTestCase("main");
            else
               tst = new SRPProtocolTestCase("main", args[0], args[1].toCharArray());

            tst.setUp();
            tst.testProtocol();
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);
        }
        finally
        {
            long end = System.currentTimeMillis();
            System.out.println("Elapsed time = "+(end - start));
        }
    }

}
