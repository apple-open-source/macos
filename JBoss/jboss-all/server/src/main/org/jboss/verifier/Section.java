package org.jboss.verifier;

/*
 * Class org.jboss.verifier.Section
 * Copyright (C) 2000  Juha Lindfors
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
 * $Id: Section.java,v 1.5 2002/05/31 13:02:14 lqd Exp $
 */

// standard imports
import java.util.StringTokenizer;
import java.util.Enumeration;
import java.util.Collections;
import java.util.Arrays;
import java.util.Iterator;
import java.text.ParseException;


/**
 * Represents a section in the EJB spec.
 *
 * @author Juha Lindfors
 * @version $Revision: 1.5 $
 * @since  JDK 1.3
 */
public class Section {

   private String[] section;
   private String info;

   /**
    * Default Constructor
    */
   public Section( String id )
   {
      try
      {
         section = parseSection( id );
      } catch ( ParseException e )
      {
         throw new IllegalArgumentException( CONSTRUCTION_ERROR );
      }
   }

   /**
    * Constructor that takes an additional String parameter which
    * gives a hint about the actual error that occured.
    */
   public Section( String id, String info )
   {
      this( id );
      this.info = info;
   }

   /*
    ********************************************************************
    *
    * PUBLIC INSTANCE METHODS
    *
    ********************************************************************
    */

   /**
    * Returns the section number by index
    */
   public String getSectionToken( int index )
   {
      if( section.length >= index )
         throw new IndexOutOfBoundsException(GET_SECTION_INDEX_ERROR);

      return section[index];
   }

   public Iterator getSectionTokens()
   {
      return Collections.unmodifiableList(Arrays.asList(section)).iterator();
   }

   /**
    * Returns the section string
    */
   public String getSection() {
      StringBuffer buffer = new StringBuffer();

      for ( int i = 0; i < section.length; ++i )
      {
         buffer.append( section[i] );
         if ( i + 1 < section.length )
            buffer.append(".");
      }

      return buffer.toString();
   }

   /**
    * String representation of this object
    */
   public String toString()
   {
      if( info != null )
      {
         return getSection() + ": " + info;
      } else
      {
         return getSection();
      }
   }

   public boolean hasInfo()
   {
      return ( info != null ) ? true : false;
   }

   public String getInfo()
   {
      return info;
   }

   /*
    ********************************************************************
    *
    * PRIVATE INSTANCE METHODS
    *
    ********************************************************************
    */

   /*
    * parses the id string into section array
    */
   private String[] parseSection( String id )
      throws ParseException
   {
      StringTokenizer tokenizer = new StringTokenizer( id, DELIMETER );
      String[] token = new String[ tokenizer.countTokens() ];

      for (int i = 0; tokenizer.hasMoreTokens(); ++i) {
         token[i] = tokenizer.nextToken();
      }

      return token;
   }


   /*
    ********************************************************************
    *
    * PRIVATE CONSTANTS
    *
    ********************************************************************
    */

   /*
    * Used by the parseSection() to tokenize the section id string
    */
   private final static String DELIMETER = ".";

   /*
    * Error messages
    */
   private final static String PARSE_SECTION_ERROR =
      "Section token cannot be longer than one character";
   private final static String GET_SECTION_INDEX_ERROR =
      "Section index too large";
   private final static String CONSTRUCTION_ERROR =
      "Cannot parse section string";
}

/*
vim:ts=3:sw=3:et
*/
