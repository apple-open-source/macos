/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.mbeancrashtest;

/**
 * @see       <related>
 * @author    <a href="mailto:david@nustiu.net">David Budworth</a>
 * @author    <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version   $$
 */


import org.jboss.system.*;
import javax.management.*;

public class Test extends ServiceMBeanSupport implements TestMBean{
   boolean crash = false;
   ObjectName name = null;
   public ObjectName preRegister(MBeanServer server, ObjectName name) throws Exception{
      crash = name.getKeyProperty("name").equals("Crash");
      log.info("crash="+crash);
      this.name=name;
      return super.preRegister(server,name);
   }
   public String getName(){
      return name.toString();
   }
   public void start() throws Exception{
      log.info("starting");
      if (crash)
         throw new Exception("Crashing on purpose");
      log.info("Started!");
      super.start();
   }
         
      
}
