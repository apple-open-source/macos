/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.compatibility;

import java.util.StringTokenizer;

/**
 * This is a compatibility tool, used to either generate serialized files from
 * loaded classes, or to test loaded classes against serialized files for
 * backwards compatibility.
 *
 * <p>
 * In order to get backwards compatibility, classes that are "contract" classes
 * need to support the java serialization api and how this works with compatibility.
 * 
 * This tool uses the following syntax:
 * java org.jboss.compatibility.CompatibilityTool make|check fully-qualified-class-name1,...
 * 
 * make:  Creates serialized versions of the fq classes given as arguments
 * check: Verifies class files by loading them into the classloader and then
 *        attempting to read back the serialized version of the file
 * 
 * Note that this client needs to have all the classes in its classpath in order for
 * it to be able to load them for both serialization and verification.
 * 
 * The use of this client requires the following for the given contract classes:
 * 
 * - That they all implement java.io.Externalizable or java.io.Serializable
 * - That they all have a public, no-args constructor
 * 
 * The classes must also provide their own serialVersionUID variable in order
 * to remain compatible between versions. When changing the source, the only way
 * for the de-serialization to work (and thus client-server communication to work)
 * is by keeping the serialVersionUID intact across changes. If unchanged and the
 * developer has _not_ provided his/her own serial version, the files will still
 * be compatible since the serialization API then generates the version itself, and
 * this should be identical to the previous version.
 * 
 * Keep in mind that this is only declarative compatibility. You can still change
 * your code so that it is incompatible, but keep the same serialVersionUID,
 * thereby tricking the serialization API into believing the classes are compatible.
 * If they really are incompatible this will result in runtime exceptions.
 * 
 *
 * @version <tt>$Revision: 1.4.2.2 $</tt>
 * @author  <a href="mailto:thomas.aardal@conduct.no">Thomas Roka-Aardal</a>.
 *   
 */
public class CompatibilityTool
{

   public static void main(String[] args)
   {

      System.out.println(
         "\n*************************\nCompatibilityTool running\n*************************\n");

      // Verify correct syntax for the client application
      if ((args.length != 2)
         || (!args[0].equalsIgnoreCase("make"))
         && (!args[0].equalsIgnoreCase("check")))
      {
         syntax();
      }

      CompatibilityToolParser parser =
         new CompatibilityToolParser(
            args[0],
            new StringTokenizer(args[1], ","));

      parser.parseArguments();
      System.exit(parser.getResult());

   }

   private static void syntax()
   {

      System.out.print(
         "Syntax: java org.jboss.compatibility.CompatibilityTool make|check file1 file2 ...\n");
      System.exit(-1);

   }

}
