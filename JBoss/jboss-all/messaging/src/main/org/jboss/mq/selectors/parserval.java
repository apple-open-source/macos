package org.jboss.mq.selectors;

//########## SEMANTIC VALUES ##########
/**
 * @created    August 16, 2001
 */
public class parserval {
   public int       ival;
   public double    dval;
   public String    sval;
   public Object    obj;

   public parserval( int val ) {
      ival = val;
   }

   public parserval( double val ) {
      dval = val;
   }

   public parserval( String val ) {
      sval = val;
   }

   public parserval( Object val ) {
      obj = val;
   }
}
