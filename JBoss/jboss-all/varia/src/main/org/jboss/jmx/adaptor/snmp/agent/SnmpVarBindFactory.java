/*
 * Copyright (c) 2003,  Intracom S.A. - www.intracom.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This package and its source code is available at www.jboss.org
**/
package org.jboss.jmx.adaptor.snmp.agent;

import java.util.Map;
import java.util.Date;
import java.util.HashMap;

import org.opennms.protocols.snmp.SnmpVarBind;
import org.opennms.protocols.snmp.SnmpObjectId;
import org.opennms.protocols.snmp.SnmpInt32;
import org.opennms.protocols.snmp.SnmpOctetString;
import org.opennms.protocols.snmp.SnmpCounter64;

import org.jboss.logging.Logger;
/**
 * <tt>SnmpVarBindFactory</tt> implements the infrastructure required to 
 * generate SNMP variable bindings from generic Object instances.
 * For each handled type (integer, string, e.t.c.) a corresponding maker class
 * is present that "knows" how to make and populate the coresponding variable 
 * binding (SnmpInt32, SnmpOctetString). The mapping between types and makers
 * is held in a hash map for optimised performance.
 *
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public class SnmpVarBindFactory
{
   /** The logger object */
   private static final Logger log = Logger.getLogger(SnmpVarBindFactory.class);
   
   /** Contains "type - maker" tupples */
   private Map makers = new HashMap();

   /** Default Maker */
   private final Maker defaultMaker = new SnmpObjectMaker();
   
   /**
    * CTOR - Initialises the factory with the known handled types and maker
    * instances
   **/        
   public SnmpVarBindFactory()
   {
      makers.put("java.lang.String", new SnmpOctetStringMaker());
      makers.put("java.lang.Integer", new SnmpInt32Maker());
      makers.put("java.lang.Long", new SnmpCounter64Maker());
      makers.put("java.math.BigInteger", new SnmpCounter64Maker());
      makers.put("java.util.Date", new SnmpDateMaker());
   }
    
   /**
    * The factory method. A lookup is performed based on the type of the
    * provided value, as this is returned by "getClass().getName()". If a
    * match is found the call is delegated to the returned maker.
   **/    
   public SnmpVarBind make(String oid, Object value)
      throws MappingFailedException
   {
      // Get value type and locate the maker
      String type = value.getClass().getName();
      Maker m = (Maker)this.makers.get(type);
        
      // Delegate where type match is found. If not use generic varbind maker
      if (m == null) {
         log.warn("Value type \"" + type + "\" for OID " + oid +
                  " encountered. Using default VarBind maker");
         
         return defaultMaker.make(oid, value);
      }
      else
         return m.make(oid, value);
   }
    
   /**
    * The generic interface that should be implemented by all makers.
   **/     
   interface Maker
   {
      public SnmpVarBind make(String oid, Object value)
         throws MappingFailedException;
   }

   /**
    * Generates unsigned integer SNMP variable bindings
   **/         
   class SnmpInt32Maker
      implements Maker
   {
      public SnmpVarBind make(String oid, Object value) 
         throws MappingFailedException
      {
         Integer i = (Integer)value;
            
         return new SnmpVarBind(new SnmpObjectId(oid), 
                                new SnmpInt32(i));
      }
   } // class SnmpInt32Maker

   /**
    * Generates unsigned long integer SNMP variable bindings
   **/         
   class SnmpCounter64Maker
      implements Maker
   {
      public SnmpVarBind make(String oid, Object value) 
         throws MappingFailedException
      {
         Long l = (Long)value;
            
         return new SnmpVarBind(new SnmpObjectId(oid), 
                                new SnmpCounter64(l.longValue()));
      }
   } // class SnmpCounter64Maker    
    
   /**
    * Generates octet string SNMP variable bindings
   **/         
   class SnmpOctetStringMaker
      implements Maker
   {
      public SnmpVarBind make(String oid, Object value)
         throws MappingFailedException
      {
         String s = (String)value;
            
         return new SnmpVarBind(new SnmpObjectId(oid),
                                new SnmpOctetString(s.getBytes()));
      }
   } // class OctetStringMaker

   /**
    * Generates octet string SNMP variable bindings from dates
   **/         
   class SnmpDateMaker
      implements Maker
   {
      public SnmpVarBind make(String oid, Object value)
         throws MappingFailedException
      {
         Date d = (Date)value;
         SnmpOctetStringMaker sMaker =  new SnmpOctetStringMaker();

         return sMaker.make(oid, d.toString());
      }
   } // class SnmpDateMaker

   /**
    * Generates octet string SNMP variable bindings from objects
   **/         
   class SnmpObjectMaker
      implements Maker
   {
      public SnmpVarBind make(String oid, Object value)
         throws MappingFailedException
      {
         SnmpOctetStringMaker sMaker =  new SnmpOctetStringMaker();

         return sMaker.make(oid, value.toString());
      }
   } // class SnmpDateMaker   
    
} // class SnmpVarBindFactory


