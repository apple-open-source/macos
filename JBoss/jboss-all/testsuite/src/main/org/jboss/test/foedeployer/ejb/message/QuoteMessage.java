/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.test.foedeployer.ejb.message;

/**
 * Message that is used by MessageTraderBean
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 */
public class QuoteMessage
   implements java.io.Serializable
{
   // Attributes ------------------------------------------------------
   private String quote;

   // Constructor -----------------------------------------------------
   public QuoteMessage(String quote)
   {
      this.quote = quote;
   }

   // Public ----------------------------------------------------------
   public String getQuote()
   {
      return quote;
   }

   public void setQuote(String quote)
   {
      this.quote = quote;
   }

   public String toString()
   {
      return "[" + getQuote() + "]";
   }
}
