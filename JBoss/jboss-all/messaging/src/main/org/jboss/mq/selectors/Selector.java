/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.selectors;

import java.util.HashMap;
import java.util.Iterator;

import javax.jms.InvalidSelectorException;
import javax.jms.JMSException;

import org.jboss.logging.Logger;

import org.jboss.mq.SpyMessage;

/**
 * This class implements a Message Selector.
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Juha Lindfors (jplindfo@helsinki.fi)
 * @author     <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author     Scott.Stark@jboss.org
 * @created    August 16, 2001
 * @version    $Revision: 1.11.2.1 $
 */
public class Selector
{
   /** The logging interface */
   static Logger cat = Logger.getLogger(Selector.class);
   
   /** The ISelectorParser implementation class */
   private static Class parserClass = SelectorParser.class;

   public HashMap identifiers;
   
   public Object result;
   
   private Class resultType;

   /**
    * Get the class that implements the ISelectorParser interface to be used by
    * Selector instances.
    */
   public static Class getSelectorParserClass()
   {
      return Selector.parserClass;
   }
   
   /**
    * Set the class that implements the ISelectorParser interface to be used by
    * Selector instances.
    * 
    * @param parserClass  the ISelectorParser implementation. This must have a
    *                     public no-arg constructor.
    */
   public static void setSelectorParserClass(Class parserClass)
   {
      Selector.parserClass = parserClass;
   }

   public Selector(String sel) throws InvalidSelectorException
   {
      identifiers = new HashMap();
      
      try
      {
         ISelectorParser bob = (ISelectorParser) parserClass.newInstance();
         result = bob.parse(sel, identifiers);
         resultType = result.getClass();
      }
      catch (Exception e)
      {
         InvalidSelectorException exception =
         new InvalidSelectorException("The selector is invalid: " + sel);
         exception.setLinkedException(e);
         throw exception;
      }
      
      // if (log.isDebugEnabled()) {
      //    log.debug("result: " + resultType + " = " + result);
      // }
   }

   public boolean test(SpyMessage.Header mes) throws JMSException
   {
      try
      {
         // Set the identifiers values
         Iterator i = identifiers.values().iterator();
         
         while (i.hasNext())
         {
            Identifier id = (Identifier)i.next();
            Object find = mes.jmsProperties.get(id.name);
            //             if (log.isDebugEnabled()) {
            //                log.debug("Identifier: " + id);
            //                log.debug("Property: " + find);
            //                if (find != null) {
            //                   log.debug("Property type: " + find.getClass());
            //                }
            //             }
            
            if (find == null)
            {
               find = getHeaderFieldReferences(mes, id.name);
            }
            
            if (find == null)
            {
               //if (log.isDebugEnabled())
               //{
               //   log.debug("Warning : missing property " + id.name);
               //}
               id.value = null;
            }
            else
            {
               Class type = find.getClass();
               if (type.equals(Boolean.class) ||
                  type.equals(String.class)  ||
                  type.equals(Double.class)  ||
                  type.equals(Float.class)   ||
                  type.equals(Integer.class) ||
                  type.equals(Long.class)    ||
                  type.equals(Short.class)   ||
                  type.equals(Byte.class))
               {
                  id.value = find;
               }
               else
               {
                  throw new Exception("Bad property type: " + type);
               }
               
               //                if (log.isDebugEnabled()) {
               //                   log.debug("SEL:" + id.name + " =>" + id.value);
               //                }
            }
         }
         
         // Compute the result of this operator
         Object res;
         
         if (resultType.equals(Identifier.class))
         {
            res = ((Identifier)result).value;
         }
         else if (resultType.equals(Operator.class))
         {
            res = ((Operator)result).apply();
         }
         else
         {
            res = result;
         }
         
         //          if (log.isDebugEnabled()) {
         //             log.debug("res: " + res);
         //             if (result != null) {
         //                log.debug("res type: " + res.getClass());
         //             }
         //          }
         
         if (res == null)
         {
            return false;
         }
         
         if (!(res.getClass().equals(Boolean.class)))
         {
            throw new Exception("Bad object type: " + res);
         }
         
         //          if (log.isDebugEnabled()) {
         //             log.debug("Selectors =>" + res);
         //          }
         
         return ((Boolean )res).booleanValue();
      }
      catch (Exception e)
      {
         cat.debug("Invalid selector: ",e);
         throw new JMSException("SELECTOR: " + e.getMessage());
      }
   }

   public boolean test(SpyMessage msg) throws JMSException {
      return test(msg.header);
   }
   
   // [JPL]
   private Object getHeaderFieldReferences(SpyMessage.Header header, String idName)
      throws JMSException
   {
      // JMS 3.8.1.1 -- Message header field references are restricted to:
      //                JMSDeliveryMode, JMSPriority, JMSMessageID,
      //                JMSTimeStamp, JMSCorrelationID and JMSType
      //
      if (idName.equals("JMSDeliveryMode"))
      {
         return new Integer(header.jmsDeliveryMode);
      }
      else if (idName.equals("JMSPriority"))
      {
         return new Integer(header.jmsPriority);
      }
      else if (idName.equals("JMSMessageID"))
      {
         return header.jmsMessageID;
      }
      else if (idName.equals("JMSTimestamp"))
      {
         return new Long(header.jmsTimeStamp);
      }
      else if (idName.equals("JMSCorrelationID"))
      {
         return header.jmsCorrelationIDString;
      }
      else if (idName.equals("JMSType"))
      {
         return header.jmsType;
      }
      else
      {
         return null;
      }
   }
}
