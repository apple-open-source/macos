/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.selectors;

import java.util.HashSet;
import org.jboss.logging.Logger;

/**
 * An operator for the selector system.
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     droy@boostmyscore.com
 * @author     Scott.Stark@jboss.org
 * @created    August 16, 2001
 * @version    $Revision: 1.8.2.1 $
 */
public class Operator
{
   int              operation;
   Object           oper1;
   Object           oper2;
   Object           oper3;

   Object           arg1;
   Object           arg2;
   Object           arg3;

   int              class1;
   int              class2;
   int              class3;
   
   // info about the regular expression
   // if this is a LIKE operator
   // (perhaps this should be a subclass)
   RegExp            re = null;

   public final static int EQUAL = 0;
   public final static int NOT = 1;
   public final static int AND = 2;
   public final static int OR = 3;
   public final static int GT = 4;
   public final static int GE = 5;
   public final static int LT = 6;
   public final static int LE = 7;
   public final static int DIFFERENT = 8;
   public final static int ADD = 9;
   public final static int SUB = 10;
   public final static int NEG = 11;
   public final static int MUL = 12;
   public final static int DIV = 13;
   public final static int BETWEEN = 14;
   public final static int NOT_BETWEEN = 15;
   public final static int LIKE = 16;
   public final static int NOT_LIKE = 17;
   public final static int LIKE_ESCAPE = 18;
   public final static int NOT_LIKE_ESCAPE = 19;
   public final static int IS_NULL = 20;
   public final static int IS_NOT_NULL = 21;
   public final static int IN = 22;
   public final static int NOT_IN = 23;

   public final static int STRING = 0;
   public final static int DOUBLE = 1;
   //DOUBLE FLOAT
   public final static int LONG = 2;
   //LONG BYTE SHORT INTEGER
   public final static int BOOLEAN = 3;

   public Operator( int operation, Object oper1, Object oper2, Object oper3 ) {
      this.operation = operation;
      this.oper1 = oper1;
      this.oper2 = oper2;
      this.oper3 = oper3;
   }

   public Operator( int operation, Object oper1, Object oper2 ) {
      this.operation = operation;
      this.oper1 = oper1;
      this.oper2 = oper2;
      this.oper3 = null;
   }

   public Operator( int operation, Object oper1 ) {
      this.operation = operation;
      this.oper1 = oper1;
      this.oper2 = null;
      this.oper3 = null;
   }

   //--- Print functions ---

   public String toString() {
      return print( "" );
   }

   public String print( String level ) {
      String st = level + operation + ":" + operationString(operation) + "(\n";

      String nextLevel = level + "  ";

      if ( oper1 == null ) {
         st += nextLevel + "null\n";
      } else if ( oper1 instanceof Operator ) {
         st += ( ( Operator )oper1 ).print( nextLevel );
      } else {
         st += nextLevel + oper1.toString() + "\n";
      }

      if ( oper2 != null ) {
         if ( oper2 instanceof Operator ) {
            st += ( ( Operator )oper2 ).print( nextLevel );
         } else {
            st += nextLevel + oper2.toString() + "\n";
         }
      }

      if ( oper3 != null ) {
         if ( oper3 instanceof Operator ) {
            st += ( ( Operator )oper3 ).print( nextLevel );
         } else {
            st += nextLevel + oper3.toString() + "\n";
         }
      }

      st += level + ")\n";

      return st;
   }


   //Operator 20
   Object is_null()
      throws Exception {
      computeArgument1();
      if ( arg1 == null ) {
         return Boolean.TRUE;
      } else {
         return Boolean.FALSE;
      }
   }

   //Operator 21
   Object is_not_null()
      throws Exception {
      computeArgument1();
      if ( arg1 != null ) {
         return Boolean.TRUE;
      } else {
         return Boolean.FALSE;
      }
   }

   //Operation 0
   Object equal()
      throws Exception {
      computeArgument1();

      if ( arg1 == null ) {
         return Boolean.FALSE;
      }

      switch ( class1 ) {
         case STRING:
         case LONG:
         case DOUBLE:
         case BOOLEAN:
            computeArgument2();
            
            if ( arg2 == null ) {
               return Boolean.FALSE;
            }
            if ( class2 != class1 ) {
               throw new Exception( "EQUAL: Bad object type" );
            }
            return new Boolean( arg1.equals( arg2 ) );
         default:
            throw new Exception( "EQUAL: Bad object type" );
      }

   }

   //Operation 1
   Object not()
      throws Exception {
      computeArgument1();

      if ( arg1 == null ) {
         return null;
      }
      if ( class1 != BOOLEAN ) {
         throw new Exception( "NOT: Bad object type" );
      }
      if ( ( ( Boolean )arg1 ).booleanValue() ) {
         return Boolean.FALSE;
      } else {
         return Boolean.TRUE;
      }
   }

   //Operation 2
   Object and()
      throws Exception {
      computeArgument1();

      if ( arg1 == null ) {
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 != BOOLEAN ) {
            throw new Exception( "AND: Bad object type" );
         }
         if ( !( ( Boolean )arg2 ).booleanValue() ) {
            return Boolean.FALSE;
         }
         return null;
      }

      if ( class1 == BOOLEAN ) {
         if ( !( ( Boolean )arg1 ).booleanValue() ) {
            return Boolean.FALSE;
         }
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 != BOOLEAN ) {
            throw new Exception( "AND: Bad object type" );
         }
         return arg2;
      }

      throw new Exception( "AND: Bad object type" );
   }

   /**
    * Operation 3
    * 
    * | OR   |   T   |   F   |   U
    * +------+-------+-------+--------
    * |  T   |   T   |   T   |   T
    * |  F   |   T   |   F   |   U
    * |  U   |   T   |   U   |   U
    * +------+-------+-------+------- 
    */
   Object or()
      throws Exception {
      short falseCounter=0;
      
      computeArgument1();
      if ( arg1 != null ) {
         if ( class1 != BOOLEAN ) {
            throw new Exception( "OR: Bad object type" );
         }
         if ( ( ( Boolean )arg1 ).booleanValue() ) {
            return Boolean.TRUE;
         } else {
            falseCounter++;
         }
      }

      computeArgument2();
      if ( arg2 != null ) {
         if ( class2 != BOOLEAN ) {
            throw new Exception( "OR: Bad object type" );
         }
         if ( ( ( Boolean )arg2 ).booleanValue() ) {
            return Boolean.TRUE;
         } else {
            falseCounter++;
         }
      }

      if( falseCounter == 2 ) {
      	return Boolean.FALSE;
      }
      
      return null;
      
   }

   //Operation 4
   Object gt()
      throws Exception {
      computeArgument1();
      if ( arg1 == null ) {
         return null;
      }

      if ( class1 == LONG ) {
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 == LONG ) {
            return new Boolean( ( ( Number )arg1 ).longValue() > ( ( Number )arg2 ).longValue() );
         }
         if ( class2 == DOUBLE ) {
            return new Boolean( ( ( Number )arg1 ).longValue() > ( ( Number )arg2 ).doubleValue() );
         }
         
      } else if ( class1 == DOUBLE ) {
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 == LONG ) {
            return new Boolean( ( ( Number )arg1 ).doubleValue() > ( ( Number )arg2 ).longValue() );
         }
         if ( class2 == DOUBLE ) {
            return new Boolean( ( ( Number )arg1 ).doubleValue() > ( ( Number )arg2 ).doubleValue() );
         }
         return Boolean.FALSE;
      }
      return Boolean.FALSE;
   }

   //Operation 5
   Object ge()
      throws Exception {
         
      computeArgument1();
      if ( arg1 == null ) {
         return null;
      }

      if ( class1 == LONG ) {
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 == LONG ) {
            return new Boolean( ( ( Number )arg1 ).longValue() >= ( ( Number )arg2 ).longValue() );
         }
         if ( class2 == DOUBLE ) {
            return new Boolean( ( ( Number )arg1 ).longValue() >= ( ( Number )arg2 ).doubleValue() );
         }
         
      } else if ( class1 == DOUBLE ) {
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 == LONG ) {
            return new Boolean( ( ( Number )arg1 ).longValue() >= ( ( Number )arg2 ).longValue() );
         }
         if ( class2 == DOUBLE ) {
            return new Boolean( ( ( Number )arg1 ).doubleValue() >= ( ( Number )arg2 ).doubleValue() );
         }
         return Boolean.FALSE;
      }
      return Boolean.FALSE;         
   }

   //Operation 6
   Object lt()
      throws Exception {
      computeArgument1();
      if ( arg1 == null ) {
         return null;
      }

      if ( class1 == LONG ) {
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 == LONG ) {
            return new Boolean( ( ( Number )arg1 ).longValue() < ( ( Number )arg2 ).longValue() );
         }
         if ( class2 == DOUBLE ) {
            return new Boolean( ( ( Number )arg1 ).longValue() < ( ( Number )arg2 ).doubleValue() );
         }
      } else if ( class1 == DOUBLE ) {
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 == LONG ) {
            return new Boolean( ( ( Number )arg1 ).doubleValue() < ( ( Number )arg2 ).longValue() );
         }
         if ( class2 == DOUBLE ) {
            return new Boolean( ( ( Number )arg1 ).doubleValue() < ( ( Number )arg2 ).doubleValue() );
         }
      }

      return Boolean.FALSE;
   }

   //Operation 7
   Object le()
      throws Exception {
      computeArgument1();
      if ( arg1 == null ) {
         return null;
      }

      if ( class1 == LONG ) {
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 == LONG ) {
            return new Boolean( ( ( Number )arg1 ).longValue() <= ( ( Number )arg2 ).longValue() );
         }
         if ( class2 == DOUBLE ) {
            return new Boolean( ( ( Number )arg1 ).longValue() <= ( ( Number )arg2 ).doubleValue() );
         }
      } else if ( class1 == DOUBLE ) {
         computeArgument2();
         if ( arg2 == null ) {
            return null;
         }
         if ( class2 == LONG ) {
            return new Boolean( ( ( Number )arg1 ).doubleValue() <= ( ( Number )arg2 ).longValue() );
         }
         if ( class2 == DOUBLE ) {
            return new Boolean( ( ( Number )arg1 ).doubleValue() <= ( ( Number )arg2 ).doubleValue() );
         }
      }
      return Boolean.FALSE;
   }

   //Operation 8
   Object different()
      throws Exception {
      computeArgument1();

      if ( arg1 == null ) {
         return Boolean.FALSE;
      }

      switch ( class1 ) {
         case DOUBLE:
         case STRING:
         case LONG:
            computeArgument2();
            if ( arg2 == null ) {
               return Boolean.FALSE;
            }
            if ( class2 != class1 ) {
               throw new Exception( "DIFFERENT: Bad object type" );
            }
            return new Boolean( !arg1.equals( arg2 ) );
         default:
            throw new Exception( "DIFFERENT: Bad object type" );
      }
   }

   //Operator 9
   Object add()
      throws Exception {
      computeArgument1();
      computeArgument2();

      if ( arg1 == null || arg2 == null ) {
         return null;
      }
      switch ( class1 ) {
         case DOUBLE:
            switch ( class2 ) {
               case DOUBLE:
                  return new Double( ( ( Number )arg1 ).doubleValue() + ( ( Number )arg2 ).doubleValue() );
               case LONG:
                  return new Double( ( ( Number )arg1 ).doubleValue() + ( ( Number )arg2 ).doubleValue() );
               default:
                  throw new Exception( "ADD: Bad object type" );
            }
         case LONG:
            switch ( class2 ) {
               case DOUBLE:
                  return new Double( ( ( Number )arg1 ).doubleValue() + ( ( Number )arg2 ).doubleValue() );
               case LONG:
                  return new Long( ( ( Number )arg1 ).longValue() + ( ( Number )arg2 ).longValue() );
               default:
                  throw new Exception( "ADD: Bad object type" );
            }
         default:
            throw new Exception( "ADD: Bad object type" );
      }
   }

   //Operator 10
   Object sub()
      throws Exception {
      computeArgument1();
      computeArgument2();

      if ( arg1 == null || arg2 == null ) {
         return null;
      }
      switch ( class1 ) {
         case DOUBLE:
            switch ( class2 ) {
               case DOUBLE:
                  return new Double( ( ( Number )arg1 ).doubleValue() - ( ( Number )arg2 ).doubleValue() );
               case LONG:
                  return new Double( ( ( Number )arg1 ).doubleValue() - ( ( Number )arg2 ).doubleValue() );
               default:
                  throw new Exception( "SUB: Bad object type" );
            }
         case LONG:
            switch ( class2 ) {
               case DOUBLE:
                  return new Double( ( ( Number )arg1 ).doubleValue() - ( ( Number )arg2 ).doubleValue() );
               case LONG:
                  return new Long( ( ( Number )arg1 ).longValue() - ( ( Number )arg2 ).longValue() );
               default:
                  throw new Exception( "SUB: Bad object type" );
            }
         default:
            throw new Exception( "SUB: Bad object type" );
      }
   }

   //Operator 11
   Object neg()
      throws Exception {
      computeArgument1();

      if ( arg1 == null ) {
         return null;
      }
      switch ( class1 ) {
         case DOUBLE:
            return new Double( -( ( Number )arg1 ).doubleValue() );
         case LONG:
            return new Long( -( ( Number )arg1 ).longValue() );
         default:
            throw new Exception( "NEG: Bad object type="+class1);
      }
   }

   //Operator 12
   Object mul()
      throws Exception {
      computeArgument1();
      computeArgument2();

      if ( arg1 == null || arg2 == null ) {
         return null;
      }
      switch ( class1 ) {
         case DOUBLE:
            switch ( class2 ) {
               case DOUBLE:
                  return new Double( ( ( Number )arg1 ).doubleValue() * ( ( Number )arg2 ).doubleValue() );
               case LONG:
                  return new Double( ( ( Number )arg1 ).doubleValue() * ( ( Number )arg2 ).doubleValue() );
               default:
                  throw new Exception( "MUL: Bad object type" );
            }
         case LONG:
            switch ( class2 ) {
               case DOUBLE:
                  return new Double( ( ( Number )arg1 ).doubleValue() * ( ( Number )arg2 ).doubleValue() );
               case LONG:
                  return new Long( ( ( Number )arg1 ).longValue() * ( ( Number )arg2 ).longValue() );
               default:
                  throw new Exception( "MUL: Bad object type" );
            }
         default:
            throw new Exception( "MUL: Bad object type" );
      }
   }

   //Operator 13
   Object div()
      throws Exception {
      //Can throw Divide by zero exception...

      computeArgument1();
      computeArgument2();

      if ( arg1 == null || arg2 == null ) {
         return null;
      }
      switch ( class1 ) {
         case DOUBLE:
            switch ( class2 ) {
               case DOUBLE:
                  return new Double( ( ( Number )arg1 ).doubleValue() / ( ( Number )arg2 ).doubleValue() );
               case LONG:
                  return new Double( ( ( Number )arg1 ).doubleValue() / ( ( Number )arg2 ).doubleValue() );
               default:
                  throw new Exception( "DIV: Bad object type" );
            }
         case LONG:
            switch ( class2 ) {
               case DOUBLE:
                  return new Double( ( ( Number )arg1 ).doubleValue() / ( ( Number )arg2 ).doubleValue() );
               case LONG:
                  return new Long( ( ( Number )arg1 ).longValue() / ( ( Number )arg2 ).longValue() );
               default:
                  throw new Exception( "DIV: Bad object type" );
            }
         default:
            throw new Exception( "DIV: Bad object type" );
      }
   }

   //Operator 14
   Object between()
      throws Exception {
      Object res = ge();
      if ( res == null ) {
         return null;
      }
      if ( !( ( Boolean )res ).booleanValue() ) {
         return res;
      }

      Object oper4 = oper2;
      oper2 = oper3;
      res = le();
      oper2 = oper4;
      return res;
   }

   //Operator 15
   Object not_between()
      throws Exception {
      Object res = lt();
      if ( res == null ) {
         return null;
      }
      if ( ( ( Boolean )res ).booleanValue() ) {
         return res;
      }

      Object oper4 = oper2;
      oper2 = oper3;
      res = gt();
      oper2 = oper4;
      return res;
   }

   //Operation 16,17,18,19
   /**
    *  Handle LIKE, NOT LIKE, LIKE ESCAPE, and NOT LIKE ESCAPE operators.
    *
    * @param  not            true if this is a NOT LIKE construct, false if this
    *      is a LIKE construct.
    * @param  use_escape     true if this is a LIKE ESCAPE construct, false if
    *      there is no ESCAPE clause
    * @return                Description of the Returned Value
    * @exception  Exception  Description of Exception
    */
   Object like( boolean not, boolean use_escape )
      throws Exception {
      Character escapeChar = null;

      computeArgument1();
      if ( arg1 == null ) {
         return null;
      }
      if ( class1 != STRING ) {
         throw new Exception( "LIKE: Bad object type" );
      }

      computeArgument2();
      if ( arg2 == null ) {
         return null;
      }
      if ( class2 != STRING ) {
         throw new Exception( "LIKE: Bad object type" );
      }

      if ( use_escape ) {
         computeArgument3();
         if ( arg3 == null ) {
            return null;
         }

         if ( class3 != STRING ) {
            throw new Exception( "LIKE: Bad object type" );
         }

         StringBuffer escapeBuf = new StringBuffer( ( String )arg3 );
         if ( escapeBuf.length() != 1 ) {
            throw new Exception( "LIKE ESCAPE: Bad escape character" );
         }

         escapeChar = new Character( escapeBuf.charAt( 0 ) );
      }

      if (re == null) {
         // the first time through we prepare the regular expression
         re = new RegExp ( (String) arg2, escapeChar);
      }
      
      boolean result = re.isMatch (arg1);
      if ( not ) {
         result = !result;
      }
      
      if (result == true) {
         return Boolean.TRUE;
      }
      else {
         return Boolean.FALSE;
      }
     
   }

   //Operator 22
   Object in()
      throws Exception {
      computeArgument1();

      if ( arg1 == null ) {
         return null;
      }
      if ( ( ( HashSet )oper2 ).contains( arg1 ) ) {
         return Boolean.TRUE;
      } else {
         return Boolean.FALSE;
      }
   }

   //Operator 23
   Object not_in()
      throws Exception {
      computeArgument1();

      if ( arg1 == null ) {
         return null;
      }
      if ( ( ( HashSet )oper2 ).contains( arg1 ) ) {
         return Boolean.FALSE;
      } else {
         return Boolean.TRUE;
      }
   }


   void computeArgument1()
      throws Exception {
      Class className = oper1.getClass();

      if ( className == Identifier.class ) {
         arg1 = ( ( Identifier )oper1 ).value;
      } else if ( className == Operator.class ) {
         arg1 = ( ( Operator )oper1 ).apply();
      } else {
         arg1 = oper1;
      }

      if ( arg1 == null ) {
         class1 = 0;
         return;
      }

      className = arg1.getClass();

      if ( className == String.class ) {
         class1 = STRING;
      } else if ( className == Double.class ) {
         class1 = DOUBLE;
      } else if ( className == Long.class ) {
         class1 = LONG;
      } else if ( className == Integer.class ) {
         class1 = LONG;
         arg1 = new Long( ( ( Integer )arg1 ).longValue() );
      } else if ( className == Short.class ) {
         class1 = LONG;
         arg1 = new Long( ( ( Short )arg1 ).longValue() );
      } else if ( className == Byte.class ) {
         class1 = LONG;
         arg1 = new Long( ( ( Byte )arg1 ).longValue() );
      } else if ( className == Float.class ) {
         class1 = DOUBLE;
         arg1 = new Double( ( ( Float )arg1 ).doubleValue() );
      } else if ( className == Boolean.class ) {
         class1 = BOOLEAN;
      } else {
         throw new Exception( "ARG1: Bad object type" );
      }
   }

   void computeArgument2()
      throws Exception {
      Class className = oper2.getClass();

      if ( className == Identifier.class ) {
         arg2 = ( ( Identifier )oper2 ).value;
      } else if ( className == Operator.class ) {
         arg2 = ( ( Operator )oper2 ).apply();
      } else {
         arg2 = oper2;
      }

      if ( arg2 == null ) {
         class2 = 0;
         return;
      }

      className = arg2.getClass();

      if ( className == String.class ) {
         class2 = STRING;
      } else if ( className == Double.class ) {
         class2 = DOUBLE;
      } else if ( className == Long.class ) {
         class2 = LONG;
      } else if ( className == Integer.class ) {
         class2 = LONG;
         arg2 = new Long( ( ( Integer )arg2 ).longValue() );
      } else if ( className == Short.class ) {
         class2 = LONG;
         arg2 = new Long( ( ( Short )arg2 ).longValue() );
      } else if ( className == Byte.class ) {
         class2 = LONG;
         arg2 = new Long( ( ( Byte )arg2 ).longValue() );
      } else if ( className == Float.class ) {
         class2 = DOUBLE;
         arg2 = new Double( ( ( Float )arg2 ).doubleValue() );
      } else if ( className == Boolean.class ) {
         class2 = BOOLEAN;
      } else {
         throw new Exception( "ARG2: Bad object type" );
      }
   }

   void computeArgument3()
      throws Exception {
      Class className = oper3.getClass();

      if ( className == Identifier.class ) {
         arg3 = ( ( Identifier )oper3 ).value;
      } else if ( className == Operator.class ) {
         arg3 = ( ( Operator )oper3 ).apply();
      } else {
         arg3 = oper3;
      }

      if ( arg3 == null ) {
         class3 = 0;
         return;
      }

      className = arg3.getClass();

      if ( className == String.class ) {
         class3 = STRING;
      } else if ( className == Double.class ) {
         class3 = DOUBLE;
      } else if ( className == Long.class ) {
         class3 = LONG;
      } else if ( className == Integer.class ) {
         class3 = LONG;
         arg3 = new Long( ( ( Integer )arg3 ).longValue() );
      } else if ( className == Short.class ) {
         class3 = LONG;
         arg3 = new Long( ( ( Short )arg3 ).longValue() );
      } else if ( className == Byte.class ) {
         class3 = LONG;
         arg3 = new Long( ( ( Byte )arg3 ).longValue() );
      } else if ( className == Float.class ) {
         class3 = DOUBLE;
         arg3 = new Double( ( ( Float )arg3 ).doubleValue() );
      } else if ( className == Boolean.class ) {
         class3 = BOOLEAN;
      } else {
         throw new Exception( "ARG3: Bad object type" );
      }
   }

   public Object apply()
      throws Exception {

      switch ( operation ) {

         case EQUAL:
            return equal();
         case NOT:
            return not();
         case AND:
            return and();
         case OR:
            return or();
         case GT:
            return gt();
         case GE:
            return ge();
         case LT:
            return lt();
         case LE:
            return le();
         case DIFFERENT:
            return different();
         case ADD:
            return add();
         case SUB:
            return sub();
         case NEG:
            return neg();
         case MUL:
            return mul();
         case DIV:
            return div();
         case BETWEEN:
            return between();
         case NOT_BETWEEN:
            return not_between();
         case LIKE:
            return like( false, false );
         case NOT_LIKE:
            return like( true, false );
         case LIKE_ESCAPE:
            return like( false, true );
         case NOT_LIKE_ESCAPE:
            return like( true, true );
         case IS_NULL:
            return is_null();
         case IS_NOT_NULL:
            return is_not_null();
         case IN:
            return in();
         case NOT_IN:
            return not_in();
      }

      throw new Exception( "Unknown operation" );
   }

   static String operationString(int operation)
   {
      String str = "Unknown";
      switch( operation )
      {
         case EQUAL:
            str = "EQUAL";
            break;
         case NOT:
            str = "NOT";
            break;
         case AND:
            str = "AND";
            break;
         case OR:
            str = "OR";
            break;
         case GT:
            str = "GT";
            break;
         case GE:
            str = "GE";
            break;
         case LT:
            str = "LT";
            break;
         case LE:
            str = "LE";
            break;
         case DIFFERENT:
            str = "DIFFERENT";
            break;
         case ADD:
            str = "ADD";
            break;
         case SUB:
            str = "SUB";
            break;
         case NEG:
            str = "NEG";
            break;
         case MUL:
            str = "MUL";
            break;
         case DIV:
            str = "DIV";
            break;
         case BETWEEN:
            str = "BETWEEN";
            break;
         case NOT_BETWEEN:
            str = "NOT_BETWEEN";
            break;
         case LIKE:
            str = "LIKE";
            break;
         case NOT_LIKE:
            str = "NOT_LIKE";
            break;
         case LIKE_ESCAPE:
            str = "LIKE_ESCAPE";
            break;
         case NOT_LIKE_ESCAPE:
            str = "NOT_LIKE_ESCAPE";
            break;
         case IS_NULL:
            str = "IS_NULL";
            break;
         case IS_NOT_NULL:
            str = "IS_NOT_NULL";
            break;
         case IN:
            str = "IN";
            break;
         case NOT_IN:
            str = "NOT_IN";
            break;
      }
      return str;
   }
}
