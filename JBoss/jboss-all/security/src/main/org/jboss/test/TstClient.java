package org.jboss.test;

import javax.naming.InitialContext;

import org.apache.log4j.Category;
import org.apache.log4j.ConsoleAppender;
import org.apache.log4j.PatternLayout;

import org.jboss.security.srp.SRPClientSession;
import org.jboss.security.srp.SRPServerInterface;
import org.jboss.security.srp.SRPParameters;
import org.jboss.logging.XLevel;

/** A simple test client that looks up the SimpleSRPServer in the RMI
registry and attempts to validate the username and password passed
on the command line.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.3.4.2 $
 */
public class TstClient
{
   public static void main(String[] args) throws Exception
   {
      String username = args[0];
      char[] password = args[1].toCharArray();
      String serviceName = args.length == 3 ? args[2] : "srp-test/SRPServerInterface";

      // Set up a simple configuration that logs on the console.
      Category root = Category.getRoot();
      root.setLevel(XLevel.TRACE);
      root.addAppender(new ConsoleAppender(new PatternLayout("%x%m%n")));

      InitialContext ctx = new InitialContext();
      SRPServerInterface server = (SRPServerInterface) ctx.lookup(serviceName);
      System.out.println("Found SRPServerInterface, "+server);
      SRPParameters params = server.getSRPParameters(username);
      System.out.println("Found params for username: " + username);
      SRPClientSession client = new SRPClientSession(username, password, params);
      byte[] A = client.exponential();
      byte[] B = server.init(username, A);
      System.out.println("Sent A public key, got B public key");
      byte[] M1 = client.response(B);
      byte[] M2 = server.verify(username, M1);
      System.out.println("Sent M1 challenge, got M2 challenge");
      if (client.verify(M2) == false)
         throw new SecurityException("Failed to validate server reply");
      System.out.println("Validation successful");
      server.close(username);
   }
}
