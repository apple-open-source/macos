/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.naming;

import java.util.Hashtable;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.Reference;
import javax.naming.RefAddr;
import javax.naming.LinkRef;
import javax.naming.spi.ObjectFactory;

import org.jboss.logging.Logger;

/**
 * Return a LinkRef based on a ThreadLocal key.
 *
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.2.4.3 $
 */
public class ENCThreadLocalKey
      implements ObjectFactory
{
   private static final Logger log = Logger.getLogger(ENCThreadLocalKey.class);

   // We need all the weak maps to make sure everything is released properly
   // and we don't have any memory leaks

   private final static ThreadLocal key = new ThreadLocal();
   private static InitialContext ctx;

   private static InitialContext getInitialContext() throws Exception
   {
      if (ctx == null)
         ctx = new InitialContext();
      return ctx;
   }

   public static void setKey(String tlkey)
   {
      key.set(tlkey);
   }

   public static String getKey()
   {
      return (String) key.get();
   }

   public Object getObjectInstance(Object obj,
         Name name,
         Context nameCtx,
         Hashtable environment)
         throws Exception
   {
      Reference ref = (Reference) obj;
      String reftype = (String) key.get();
      boolean trace = log.isTraceEnabled();

      if (reftype == null)
      {
         if (trace)
            log.trace("using default in ENC");
         reftype = "default";
      }

      RefAddr addr = ref.get(reftype);
      if (addr == null)
      {
         if (trace)
            log.trace("using default in ENC");
         addr = ref.get("default"); // try to get default linking
      }
      if (addr != null)
      {
         String target = (String) addr.getContent();
         if (trace)
            log.trace("found Reference " + reftype + " with content " + target);
         return new LinkRef(target);
      }
      return null;
   }

}
