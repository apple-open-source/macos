package org.jboss.test.proxycompiler;

public class Util {

   private Util() { }

   public final static String getStringRepresentation(int i,
                                                      Object ref,
                                                      int[] ints,
                                                      Object[] objectRefs) {

       StringBuffer b = new StringBuffer();
       b.append("i = " + i + ",");
       b.append("ref = " + ref + ",");
       b.append("ints = {");
       for(int j = 0; j < ints.length; j++) {
          b.append("[" + j + "]=" + ints[j]);
       }
       b.append("},");
       b.append("objectRefs = {");
       for(int j = 0; j < objectRefs.length; j++) {
          b.append("[" + j + "]=" + objectRefs[j]);
       }
       b.append("}");
 
       return b.toString();

   }
}

