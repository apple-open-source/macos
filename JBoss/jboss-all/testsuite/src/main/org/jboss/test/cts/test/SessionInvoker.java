package org.jboss.test.cts.test;

import org.jboss.test.cts.interfaces.StrictlyPooledSession;
import org.jboss.test.cts.interfaces.StrictlyPooledSessionHome;

import org.apache.log4j.Category;
import EDU.oswego.cs.dl.util.concurrent.CountDown;

/** Invoker thread for StatelessSession tests.
* @author Scott.Stark@jboss.org
* @version $Revision: 1.1.2.1 $
*/
public class SessionInvoker extends Thread
{
   StrictlyPooledSessionHome home;
   Category log;
   int id;
   CountDown done;
   Exception runEx;
   public SessionInvoker(StrictlyPooledSessionHome home, int id, CountDown done,
         Category log)
   {
      super("SessionInvoker#"+id);
      this.home = home;
      this.id = id;
      this.done = done;
      this.log = log;
   }
   public void run()
   {
      log.debug("Begin run, this="+this);
      try
      {
         StrictlyPooledSession session = home.create();
         session.methodA();
         session.remove();
      }
      catch(Exception e)
      {
         runEx = e;
      }
      done.release();
      log.debug("End run, this="+this);
   }

}
