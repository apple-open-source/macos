/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 * 
 */

package org.jboss.net.uddi.entities;

public class BusinessService
{
	public void addBinding( Binding binding )
	{
		/* Not implemented */
		throw new RuntimeException( "Not implemented yet" );
	}
	
	public void addCategory( Category category )
	{
		/* Not implemented */
		throw new RuntimeException( "Not implemented yet" );
	}
	
	public String getBinding( String bindingKey )
	{
		return this.bindingKey;
	}
	
	public String getBusinessKey( )
	{
		return this.businessKey;
	}
	
	public void setBusinessKey( String businessKey )
	{
		this.businessKey = businessKey;
	}
	
	public String getDescription( )
	{
		return this.description;
	}
	
	public void setDescription( String description )
	{
		this.description = description;
	}
	
	public String getName( )
	{
		return this.name;
	}
	
	public void setName( String name )
	{
		this.name = name;
	}
	
	/** Member variables **/
	private String name;
	private String description;
	private String businessKey;
	private String bindingKey;
	
}
