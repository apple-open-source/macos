package org.jboss.metadata;

/**
 *
 * Provides meta-data for method-attributes
 * <method-attributes>
 * <method>
 * <method-name>get*</method-name>
 * <read-only>true</read-only>
 * <idempotent>true</idempotent>
 * </method>
 * </method-attributes>
 *
 * @author <a href="pete@subx.com">Peter Murray</a>
 *
 * @version $Revision: 1.1 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2001/04/10: peter</b>
 *  <ol>
 *  <li>Initial revision
 *  </ol>
 */
public class MethodAttributes
{
    String pattern;
    boolean readOnly;
    boolean idempotent;

    public static MethodAttributes kDefaultMethodAttributes;

    static
    {
	kDefaultMethodAttributes = new MethodAttributes();
	kDefaultMethodAttributes.pattern = "*";
	kDefaultMethodAttributes.readOnly = false;
	kDefaultMethodAttributes.idempotent = false;
    }

    public boolean patternMatches(String methodName)
    {
	int ct, end;

	end = pattern.length();

	if(end > methodName.length())
	    return false;

	for(ct = 0; ct < end; ct ++)
	{
	    char c = pattern.charAt(ct);
	    if(c == '*')
		return true;
	    if(c != methodName.charAt(ct))
		return false;
	}
	return ct == methodName.length();
    }

    public boolean isReadOnly()
    {
	return readOnly;
    }

    public boolean isIdempotent()
    {
	return idempotent;
    }
}
