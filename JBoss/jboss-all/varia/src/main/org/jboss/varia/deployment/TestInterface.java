package org.jboss.varia.deployment;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>6 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public interface TestInterface
{
   public void doSimple();
   public int returnInt();
   public String returnString();
   
   public String getROString();
   
   public String getRWString();
   public void setRWString(String bla);
   
   public void setWOString(String bla);
   
   public int getROInt();
   
   public int getRWInt();
   public void setRWInt(int bla);
   
   public void setWOInt(int bla);
   
}
