package org.jboss.test.jmx.xmbean;

import java.util.Date;
import org.jboss.logging.Logger;

/** An alternate non-xdoclet JBoss model mbean
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class User2
{
   private Logger log = Logger.getLogger(User2.class);

   private String attr1;
   private int attr2;

   public User2()
   {
   }
   public User2(String attr1)
   {
      this.attr1 = attr1;
   }

   public Integer getHashCode()
   {
      return new Integer(hashCode());
   }

   public String getAttr1()
   {
      return attr1;
   }
   public void setAttr1(String attr1)
   {
      this.attr1 = attr1;
   }

   public int getAttr2()
   {
      return attr2;
   }
   public void setAttr2(int attr2)
   {
      this.attr2 = attr2;
   }

   public void start() throws Exception
   {
      log.info("Started");
   }
   public void stop() throws Exception
   {
      log.info("Stopped");
   }

   public void noop()
   {
   }
   public String echoDate(String prefix)
   {
      return prefix + ": " + new Date();
   }

}
