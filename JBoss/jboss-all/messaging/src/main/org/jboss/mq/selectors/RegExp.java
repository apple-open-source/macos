/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.selectors;

import gnu.regexp.RE;

/**
 * Regular expressions to support the selector LIKE operator.
 *
 * @created Dec. 7, 2001
 * @version <tt>$Revision: 1.1 $</tt>
 *
 * @author Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author droy@boostmyscore.com
 * @author Scott.Stark@jboss.org
 * @author Loren Rosen
 */
public class RegExp 
{
   protected RE re;
         
   public RegExp (String pattern, Character escapeChar)
      throws Exception 
   {
      re = new RE(adjustPattern(pattern, escapeChar), 0,
                  gnu.regexp.RESyntax.RE_SYNTAX_POSIX_MINIMAL_BASIC);
   }
    
   public boolean isMatch (Object target) 
   {
      return re.isMatch (target);
   }
    
   protected String adjustPattern (String pattern, Character escapeChar) 
      throws Exception 
   {
      
      // StringBuffer patternBuf = new StringBuffer( pattern );
      int patternLen = pattern.length();
      StringBuffer REpattern = new StringBuffer(patternLen + 10);
      boolean useEscape = (escapeChar != null);
      char escape = Character.UNASSIGNED;
      if (useEscape) {
         escape = escapeChar.charValue();
      }
      
      REpattern.append ('^');

      for ( int i = 0; i < patternLen; i++ ) {
         boolean escaped = false;
         char c = pattern.charAt( i );

         if ( useEscape && escape == c ) {
            i++;
            if ( i < patternLen ) {
               escaped = true;
               c = pattern.charAt( i );
            } else {
               throw new Exception( "LIKE ESCAPE: Bad use of escape character" );
            }
         }

         // Match characters, or escape ones special to the underlying
         // regex engine
         switch ( c ) {
         case '_':
            if ( escaped ) {
               REpattern.append( c );
            } else {
               REpattern.append( '.' );
            }
            break;
         case '%':
            if ( escaped ) {
               REpattern.append( c );
            } else {
               REpattern.append( ".*" );
            }
            break;
         case '*':
         case '.':
         case '\\':
         case '^':
         case '$':
         case '[':
         case ']':
            //                      case '(':
            //                      case ')':
            //                      case '+':
            //                      case '?':
            //                      case '{':
            //                      case '}':
            //                      case '|':
            REpattern.append( "\\");
            REpattern.append ( c );
            break;
         default:
            REpattern.append( c );
            break;
         }
      }

      REpattern.append( '$' );
      return REpattern.toString();
   }
}
 
