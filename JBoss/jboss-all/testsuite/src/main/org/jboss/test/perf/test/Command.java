/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.perf.test;
// Command.java
import org.jboss.test.perf.interfaces.*;

import java.util.*;

public class Command {

  public static void main(String[] args) throws Exception {
    
    if(args.length != 4) {
      System.out.println("Usage: vbj Command {create,remove} <jndi-name> <low-count> <high-count>");
      return;
    }

    String command = args[0];
    String jndiName = args[1];
    int low  = Integer.parseInt(args[2]);
    int high = Integer.parseInt(args[3]);

    javax.naming.Context context = new javax.naming.InitialContext(); 

    Object ref = context.lookup("Session");
    SessionHome sessionHome = (SessionHome) ref;
    /** CHANGES: WebLogic does not support PortableRemoteObject
     **
      //(SessionHome) javax.rmi.PortableRemoteObject.narrow(ref, SessionHome.class);
     **/

    Session session = sessionHome.create(jndiName);

    if(command.equalsIgnoreCase("create")) {
      session.create(low, high);
    }
    else if(command.equalsIgnoreCase("remove")) {
      session.remove(low, high);
    }
    else {
      System.err.println("Invalid command: " + command);
    }

    session.remove();

  }

}
