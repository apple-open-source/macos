/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 package org.jboss.compatibility;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.StringTokenizer;
import java.util.Enumeration;
import java.util.jar.JarFile;
import java.util.zip.ZipEntry;

/**
 * This is the class in which most of the action in compatibility is performed.
 * The methods in this class are meant to be called from the CompatibilityTool, 
 * so see that class for more information about what this class does.
 * 
 * @see org.jboss.compatibility.CompatibilityTool
 * 
 * @version <tt>$Revision: 1.2.2.1 $</tt>
 * @author  <a href="mailto:thomas.aardal@conduct.no">Thomas Roka-Aardal</a>.
 * 
 */
public class CompatibilityToolParser
{

   boolean foundExternalizable = false;
   boolean foundSerializable = false;
   private StringBuffer output = new StringBuffer();
   int result = 0;

   StringTokenizer arguments = null;
   String command;

   // Set the local properties from the arguments on the command line
   public CompatibilityToolParser(String command, StringTokenizer arguments)
   {
      this.command = command;
      this.arguments = arguments;
   }

    public void parseArguments()
    {

	String argument = null;

	while(arguments.hasMoreTokens()) {

	    argument = arguments.nextToken().trim();

	    if(argument.endsWith(".jar"))
		{
                    try
			{
			    JarFile jar = new JarFile(argument);
			    Enumeration enum = jar.entries();
			    while(enum.hasMoreElements())
				{
				    String zipEntryName = ((ZipEntry) enum.nextElement()).getName();
				    if(zipEntryName.endsWith(".class"))
					{
					    // Replace "/" with "."
					    zipEntryName = zipEntryName.replace('/', '.');

					    // Strip away .class from name (not necessary)
					    zipEntryName = zipEntryName.substring(0, zipEntryName.length() - ".class".length());
					    parseFile(zipEntryName);
					}
				}
			}
		    catch(IOException ioEx)
			{
			    output.append("Could not read jar file: " + argument + ", continuing");
			    result = -1;
			}
		} else
		{
		    parseFile(argument);
		}

	}

	System.out.print(output.toString());

    }

   public void parseFile(String _argument)
   {

      Class argClass = null;
      String serializedClassName = null;

         foundExternalizable = false;
         foundSerializable = false;
         serializedClassName = new String(_argument + ".ser");

         // In any case we need to load the argument classes, do this
         // using reflection.
         try
         {
            // Load the class using this class' classloader, and initialize it using the
            // public no-args constructor.
            output.append("[" + _argument + "] ");
            argClass =
               Class.forName(
                  _argument,
                  true,
                  CompatibilityToolParser.class.getClassLoader());

            // Verify that the class implements the Externalizable interface. This
            // is a requirement so that we can persist (write to disk) an instance
            // of the class, and be able to read it back from a serialized state into
            // an instance.
            findInterfaces(argClass.getInterfaces());

            if (!(foundExternalizable || foundSerializable))
            {
               // A contract class didn't implement java.io.Externalizable or java.io.Serializable,
               // which is a requirement, so exit.
               output.append(
                  "-> Contract class does not implement Externalizable or Serializable!\n");
               result = -1;
            } else
            {
               // The contract class correctly implemented java.io.Externalizable or
               // java.io.Serializable.
               output.append(
                  foundExternalizable
                     ? "-> Externalizable, "
                     : "-> Serializable, ");

               // See what it is we were called to do (make or check)
               if (command.equalsIgnoreCase("make"))
               {
                  serializeAndWriteFile(serializedClassName, argClass);
               }
               // check existing serialized file
               else
               {
                  checkSerializedFile(serializedClassName);
               }
            }

         } catch (ClassNotFoundException e)
         {
            // Class isn't in classpath, give a meaningful response.
            output.append("-> Contract class not in classpath!\n");
            result = -1;
         } catch (Throwable e)
         {
            output.append(
               "-> Could not load class, dependent classes may not be in classpath.\n");
            result = -1;
         }

   }

   /** 
    * Parse the list of interfaces and check if either 
    * java.io.Externalizable or java.io.Serializable are implemented
    *
    * @param argClassInterfaces list of interfaces
    */
   private void findInterfaces(Class[] argClassInterfaces)
   {
      for (int cnt2 = 0; cnt2 < argClassInterfaces.length; cnt2++)
      {
         if (argClassInterfaces[cnt2]
            .getName()
            .equals("java.io.Externalizable"))
         {
            foundExternalizable = true;
            break;
         } else if (
            argClassInterfaces[cnt2].getName().equals("java.io.Serializable"))
         {
            foundSerializable = true;
            break;
         }
      }
   }

   /** 
    * Write a class to disk
    * @param filename 
    * @param argClass 
    */
   private void serializeAndWriteFile(String filename, Class argClass)
   {
      FileOutputStream outStream;
      ObjectOutputStream objectOutStream;
      File partialFile;

      // Create serialized files (and overwrite existing ones)
      try
      {
         outStream = new FileOutputStream(filename);
         try
         {
            objectOutStream = new ObjectOutputStream(outStream);
            try
            {
               // We use the defined public no-args constructor that is
               // required by the serialization API to create the instance
               // that is written to disk. When implementing the Externalizable interface,
               // only the identity (the serialVersionUID) is written to disk.
               objectOutStream.writeObject(argClass.newInstance());
               output.append(" serialized.\n");

            } catch (InstantiationException in)
            {
               output.append(
                  " could not instantiate class using a public, no-args constructor\n");

               // Remove the partially written file
               partialFile = new File(filename);
               try
               {
                  objectOutStream.close();
                  outStream.close();
                  partialFile.delete();
               } catch (Exception ignore)
               {
               }

               result = -1;
            } catch (IllegalAccessException ia)
            {
               output.append(" could not access class.\n");
               result = -1;
            } catch (Throwable e)
            {
               output.append(
                  " could not create instance - dependent classes may not be in classpath.\n");

               // Remove the partially written file
               partialFile = new File(filename);

               try
               {
                  objectOutStream.close();
                  outStream.close();
                  partialFile.delete();
               } catch (Exception ignore)
               {
               }

               result = -1;
            }
         } catch (IOException io)
         {
            output.append(" could not write to file!\n");
            result = -1;
         }
      } catch (FileNotFoundException f)
      {
         output.append(" file exists, but cannot be overwritten!\n");
         result = -1;
      }
   }

   /** 
    * Try to create an object of the serialized file. 
    * If success, no errors are found..
    * @param filename name of serialized class
    */
   private void checkSerializedFile(String filename)
   {
      FileInputStream inStream;
      ObjectInputStream objectInStream;
      Object serializedObject;

      // Verify compiled classes' version against serialized files
      try
      {
         inStream = new FileInputStream(filename);

         try
         {
            objectInStream = new ObjectInputStream(inStream);

            // Read back the object from a serialized state (note that
            // this also implicitly calls the public, no-args constructor
            // that must be available.
            serializedObject = objectInStream.readObject();

            // No exception, so things must have gone okay. Continue.
            output.append(" COMPATIBLE\n");

         } catch (IOException io)
         {
            output.append(
               " serial id NOT COMPATIBLE! You are breaking a contract class!\n");
         }

      } catch (FileNotFoundException f1)
      {
         output.append(
            " Class added, does not break existing compatibility.\n");
      } catch (ClassNotFoundException e)
      {
         // It shouldnt be possible to come here.. - i think..
         output.append("-> Error when creating object from serialized file.\n");
         result = -1;
      }
   }

   public int getResult()
   {
      return result;
   }

}
