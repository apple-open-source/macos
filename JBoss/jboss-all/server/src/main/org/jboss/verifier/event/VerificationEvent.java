package org.jboss.verifier.event;

/*
 * Class org.jboss.verifier.event.VerificationEvent
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
 * $Id: VerificationEvent.java,v 1.10 2002/05/31 13:02:14 lqd Exp $
 */


// standard imports
import java.util.EventObject;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;


// non-standard class dependencies
import org.jboss.verifier.Section;


/**
 *
 * @author 	Juha Lindfors   (jplindfo@helsinki.fi)
 * @version $Revision: 1.10 $
 * @since  	JDK 1.3
 */
public class VerificationEvent extends EventObject {

    public static final String WARNING  = "WARNING";
    public static final String OK       = "OK";

    private boolean isOk      = false;
    private boolean isWarning = false;

    /*
     * Contains a short, one line message for this event.
     */
    private String message  = "<undefined>";
    private String beanName = "<unnamed>";
    private Method method   = null;
    private String section  = null;
    private String info = null;

/*
 *************************************************************************
 *
 *      PUBLIC INSTANCE METHODS
 *
 *************************************************************************
 */

   /*
    * Constructor
    */
   public VerificationEvent( VerificationEventGenerator source )
   {
      super(source);
   }

   public VerificationEvent( VerificationEventGenerator source,
      String message)
   {
      this(source);
      setMessage(message);
   }

   public void setState(String state)
   {
      if( WARNING.equalsIgnoreCase(state) )
      {
         isWarning = true;
         isOk = false;
      }
      else if( OK.equalsIgnoreCase(state) )
      {
         isOk = true;
         isWarning = false;
      }
      else
      {
         throw new IllegalArgumentException( STATE_NOT_RECOGNIZED + ": "
            + state);
      }
   }

   public boolean isOk()
   {
      return isOk;
   }

   public boolean isWarning()
   {
      return isWarning;
   }

   public void setMessage( String msg )
   {
      this.message = msg;
   }

   public void setName( String name )
   {
      this.beanName = name;
   }

   public void setSection( Section section )
   {
      this.section = section.getSection();

      if( section.hasInfo() )
         this.info = section.getInfo();
   }

   public void setMethod(Method method)
   {
      if( method == null )
         return;

      this.method = method;
   }

   public String getMessage()
   {
      return beanName + ": " + message;
   }

   public String getVerbose()
   {
      StringBuffer buf = new StringBuffer(512);
      String linebreak = System.getProperty("line.separator");

      buf.append(linebreak + "Bean   : " + beanName + linebreak);

      if( method != null )
      {
         String returnType = getShortClassName( method.getReturnType() );

         Class[] arguments = method.getParameterTypes();
         String arglist = getCommaSeparatedList( getShortClassNames(
            arguments) );

         Class[] exceptions = method.getExceptionTypes();
         String  exclist = getCommaSeparatedList(getShortClassNames(
            exceptions) );

         buf.append( "Method : " + Modifier.toString(method.getModifiers()) +
            " " + returnType        + " " +
            method.getName()  + "(" +
            arglist           + ")");

         if ( exclist.length() > 0 )
            buf.append(" throws " + exclist.toString());

         buf.append(linebreak);
      }

      int offset = section.lastIndexOf(".");
      if ( !Character.isDigit(section.charAt(offset+1)) )
         buf.append("Section: " + section.substring(0, offset)  + linebreak);
      else
         buf.append("Section: " + section + linebreak);

      buf.append( "Warning: " );
      if( message != null )
      {
         buf.append( message + linebreak );
      }
      else
      {
         buf.append( "No warning message found, please file a Bug " +
            "report." );
      }

      if( info != null )
         buf.append("Info   : " + info + linebreak );

      return buf.toString();
   }

   public String getName()
   {
      return beanName;
   }

/*
 *************************************************************************
 *
 *      PRIVATE INSTANCE METHODS
 *
 *************************************************************************
 */
   private String[] getShortClassNames( Class[] c )
   {
      String[] names = new String[c.length];

      for (int i = 0; i < c.length; ++i)
         names[i] = getShortClassName( c[i] );

      return names;
   }

   /*
    * Returns class name without package path
    */
   private String getShortClassName( Class c )
   {
      String className = c.getName();
      int len = className.length();
      int offset = className.lastIndexOf( "." );

      String name = "";

      if (offset == -1)
         name = className;
      else
         name = className.substring(offset+1, len);

      return name;
   }

   /*
    * builds a comma separated string list of objects
    */
   private String getCommaSeparatedList( Object[] list )
   {
      if (list == null || list.length <= 0)
         return "";

      if (list.length == 1)
         return list[0].toString();

      StringBuffer buf = new StringBuffer( 256 );
      buf.append( list[0] );

      for (int i = 1; i < list.length; ++i)
      {
         buf.append( ", " );
         buf.append( list[i] );
      }

      return buf.toString();
   }

    /*
     * String constants
     */
   private final static String STATE_NOT_RECOGNIZED = "Unknown event state";
}
/*
vim:ts=3:sw=3:et
*/
