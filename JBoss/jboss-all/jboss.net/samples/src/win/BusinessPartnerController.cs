/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

using System;
using JBoss_Net_Sample.businessPartner;
using System.Windows.Forms;

namespace org.jboss.net.samples.store
{

	/** 
	 * <summary>
	 * Controller for Reading and Manipulating BusinessPartner through a ListView.
	 * </summary>
	 * @author jung
	 * @created 21.03.2002
	 * @version $Revision: 1.1 $
	 */

	public class BusinessPartnerController : ListController
	{
		/** references a stateless remote service */
		BusinessPartnerServiceService service=new BusinessPartnerServiceService();

		public BusinessPartnerController(SampleView parent,ListView view) : base(parent,view)
		{
		}

		
		/** creates a new businesspartner */
		public override Object create() 
		{
			BusinessPartnerDialogue myDialog=new BusinessPartnerDialogue();
			myDialog.ShowDialog(parent);
			/** interface service to create new entity for dialogue text */
			return service.create(myDialog.getText());
		}

		/** updates the given target object */
		public override void update(Object target) 
		{
			service.update((BusinessPartner) target);
		}

		/** retrieve current set of businesspartner */
		public override Object[] retrieveObjects() 
		{
			return service.findAll();
		}

		/** deletes a businesspartner */
		public override void delete(Object target) 
		{
			service.delete((BusinessPartner) target);
		}

		/** creates a list view item for the given target as a businesspartner */
		public override ListViewItem createListItem(Object target) 
		{
			BusinessPartner bp=(BusinessPartner) target;
			ListViewItem bpItem= new ListViewItem(bp.name,7);
			if(bp.address!=null) 
			{
				bpItem.SubItems.Add(bp.address.city);
				if(bp.address.phoneNumber!=null) 
				{
					bpItem.SubItems.Add(bp.address.phoneNumber.areaCode.ToString());
					bpItem.SubItems.Add(bp.address.phoneNumber.exchange);
					bpItem.SubItems.Add(bp.address.phoneNumber.number);
				} 
				else 
				{
					for(int count=0;count<3;count++) 
					{
						bpItem.SubItems.Add("");
					}
				}
				bpItem.SubItems.Add(bp.address.state.ToString());
				bpItem.SubItems.Add(bp.address.streetName);
				bpItem.SubItems.Add(bp.address.streetNum.ToString());
				bpItem.SubItems.Add(bp.address.zip.ToString());
			} 
			else 
			{
				for(int count=0;count<8;count++) 
				{
					bpItem.SubItems.Add("");
				}
			}

			return bpItem;
		}


		
	
		/** spawns an editor for businesspartners */
		public override void edit(Object target) 
		{
			BusinessPartnerEditor myEditor=new BusinessPartnerEditor((BusinessPartner)target,this);
			myEditor.Show();
		}

		/** this is called to re-arrange the view */
		public override bool arrange()  
		{
			base.arrange();
			view.Columns.Add("BusinessPartner", 90, System.Windows.Forms.HorizontalAlignment.Left);
			view.Columns.Add("Address.City", 70, System.Windows.Forms.HorizontalAlignment.Left);
			view.Columns.Add("Address.Phone.AreaCode", 140, System.Windows.Forms.HorizontalAlignment.Left);
			view.Columns.Add("Address.Phone.Exchange", 140, System.Windows.Forms.HorizontalAlignment.Left);
			view.Columns.Add("Address.Phone.Number", 125, System.Windows.Forms.HorizontalAlignment.Left);
			view.Columns.Add("Address.State", 80, System.Windows.Forms.HorizontalAlignment.Left);
			view.Columns.Add("Address.StreetName", 110, System.Windows.Forms.HorizontalAlignment.Left);
			view.Columns.Add("Address.StreetNum", 105, System.Windows.Forms.HorizontalAlignment.Left);
			view.Columns.Add("Address.Zip", 70, System.Windows.Forms.HorizontalAlignment.Left);
			return true;
		}

	
	} // BusinessPartnerController
} // namespace
