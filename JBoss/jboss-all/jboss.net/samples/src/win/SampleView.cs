using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using System.Data;

namespace org.jboss.net.samples.store
{
	/** 
	 * A sample application that should demonstrate 
	 * advanced .Net<->JBoss interoperability
	 */

	public class SampleView : System.Windows.Forms.Form
	{
		#region Windows Form Designer generated code
		
		private System.Windows.Forms.PictureBox pictureBox1;
		private System.ComponentModel.IContainer components;
		private System.Windows.Forms.ToolBar toolBar1;
		private System.Windows.Forms.MenuItem menuItem1;
		private System.Windows.Forms.MenuItem menuItem2;
		private System.Windows.Forms.MenuItem menuItem3;
		private System.Windows.Forms.MenuItem menuItem4;
		private System.Windows.Forms.MenuItem menuItem5;
		private System.Windows.Forms.MenuItem menuItem10;
		private System.Windows.Forms.MenuItem menuItem11;
		private System.Windows.Forms.MenuItem menuItem12;
		private System.Windows.Forms.MenuItem menuItem13;
		private System.Windows.Forms.MenuItem menuItem14;
		private System.Windows.Forms.MenuItem menuItem15;
		private System.Windows.Forms.MenuItem menuItem16;
		private System.Windows.Forms.MenuItem menuItem17;
		private System.Windows.Forms.MenuItem menuItem18;
		private System.Windows.Forms.MenuItem menuItem19;
		private System.Windows.Forms.MenuItem menuItem20;
		private System.Windows.Forms.MenuItem menuItem21;
		private System.Windows.Forms.MenuItem menuItem22;
		private System.Windows.Forms.MenuItem menuItem23;
		private System.Windows.Forms.MenuItem menuItem24;
		private System.Windows.Forms.MenuItem menuItem25;
		private System.Windows.Forms.MenuItem menuItem26;
		private System.Windows.Forms.MenuItem menuItem27;
		private System.Windows.Forms.MenuItem menuItem28;
		private System.Windows.Forms.MenuItem menuItem29;
		private System.Windows.Forms.StatusBar statusBar1;
		private System.Windows.Forms.StatusBarPanel statusBarPanel1;
		private System.Windows.Forms.StatusBarPanel statusBarPanel2;
		private System.Windows.Forms.StatusBarPanel statusBarPanel3;
		private System.Windows.Forms.ToolBarButton toolBarButton1;
		private System.Windows.Forms.ToolBarButton toolBarButton2;
		private System.Windows.Forms.ToolBarButton toolBarButton3;
		private System.Windows.Forms.ToolBarButton toolBarButton4;
		private System.Windows.Forms.ToolBarButton toolBarButton5;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.TreeView treeView1;
		private System.Windows.Forms.ListView listView1;
		private System.Windows.Forms.MenuItem editItem;
		
		#endregion

		/** the currently active controller class under the listview */
		private ListController currentController;
		private System.Windows.Forms.MainMenu menu;
		private System.Windows.Forms.ImageList icons;
		private System.Windows.Forms.ComboBox comboBox1;
		/** controller for items */
		private ItemController itemController;
		private System.Windows.Forms.ColumnHeader columnHeader1;
		private System.Windows.Forms.ContextMenu itemMenu;
		private BusinessPartnerController businessPartnerController;
		
		/** constructs a new sampleform */
		public SampleView()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			// create the item controller and attach it to this
			// form and listview
			editItem.Text="Edit";
			itemController=new ItemController(this,listView1);
			businessPartnerController=new BusinessPartnerController(this,listView1);

			// we register for add and delete events
			toolBar1.ButtonClick+=new System.Windows.Forms.ToolBarButtonClickEventHandler(this.Add_Item);
			this.listView1.KeyUp+=new System.Windows.Forms.KeyEventHandler(this.Key_Released);
		}


		#region Windows Form Designer Generated code

		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if (components != null) 
				{
					components.Dispose();
				}
			}
			base.Dispose( disposing );
		}

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			System.Resources.ResourceManager resources = new System.Resources.ResourceManager(typeof(SampleView));
			this.pictureBox1 = new System.Windows.Forms.PictureBox();
			this.menu = new System.Windows.Forms.MainMenu();
			this.menuItem1 = new System.Windows.Forms.MenuItem();
			this.menuItem12 = new System.Windows.Forms.MenuItem();
			this.menuItem13 = new System.Windows.Forms.MenuItem();
			this.menuItem14 = new System.Windows.Forms.MenuItem();
			this.menuItem15 = new System.Windows.Forms.MenuItem();
			this.menuItem16 = new System.Windows.Forms.MenuItem();
			this.menuItem17 = new System.Windows.Forms.MenuItem();
			this.menuItem18 = new System.Windows.Forms.MenuItem();
			this.menuItem2 = new System.Windows.Forms.MenuItem();
			this.menuItem19 = new System.Windows.Forms.MenuItem();
			this.menuItem20 = new System.Windows.Forms.MenuItem();
			this.menuItem21 = new System.Windows.Forms.MenuItem();
			this.menuItem22 = new System.Windows.Forms.MenuItem();
			this.menuItem23 = new System.Windows.Forms.MenuItem();
			this.menuItem3 = new System.Windows.Forms.MenuItem();
			this.menuItem24 = new System.Windows.Forms.MenuItem();
			this.menuItem25 = new System.Windows.Forms.MenuItem();
			this.menuItem4 = new System.Windows.Forms.MenuItem();
			this.menuItem5 = new System.Windows.Forms.MenuItem();
			this.menuItem10 = new System.Windows.Forms.MenuItem();
			this.menuItem28 = new System.Windows.Forms.MenuItem();
			this.menuItem29 = new System.Windows.Forms.MenuItem();
			this.menuItem11 = new System.Windows.Forms.MenuItem();
			this.menuItem26 = new System.Windows.Forms.MenuItem();
			this.menuItem27 = new System.Windows.Forms.MenuItem();
			this.toolBar1 = new System.Windows.Forms.ToolBar();
			this.toolBarButton1 = new System.Windows.Forms.ToolBarButton();
			this.toolBarButton2 = new System.Windows.Forms.ToolBarButton();
			this.toolBarButton3 = new System.Windows.Forms.ToolBarButton();
			this.toolBarButton4 = new System.Windows.Forms.ToolBarButton();
			this.toolBarButton5 = new System.Windows.Forms.ToolBarButton();
			this.icons = new System.Windows.Forms.ImageList(this.components);
			this.statusBar1 = new System.Windows.Forms.StatusBar();
			this.statusBarPanel1 = new System.Windows.Forms.StatusBarPanel();
			this.statusBarPanel2 = new System.Windows.Forms.StatusBarPanel();
			this.statusBarPanel3 = new System.Windows.Forms.StatusBarPanel();
			this.panel1 = new System.Windows.Forms.Panel();
			this.listView1 = new System.Windows.Forms.ListView();
			this.columnHeader1 = new System.Windows.Forms.ColumnHeader();
			this.itemMenu = new System.Windows.Forms.ContextMenu();
			this.editItem = new System.Windows.Forms.MenuItem();
			this.treeView1 = new System.Windows.Forms.TreeView();
			this.comboBox1 = new System.Windows.Forms.ComboBox();
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel1)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel2)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel3)).BeginInit();
			this.panel1.SuspendLayout();
			this.SuspendLayout();
			// 
			// pictureBox1
			// 
			this.pictureBox1.Anchor = (System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right);
			this.pictureBox1.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.pictureBox1.Cursor = System.Windows.Forms.Cursors.Hand;
			this.pictureBox1.Image = ((System.Drawing.Bitmap)(resources.GetObject("pictureBox1.Image")));
			this.pictureBox1.Location = new System.Drawing.Point(416, 0);
			this.pictureBox1.Name = "pictureBox1";
			this.pictureBox1.Size = new System.Drawing.Size(152, 32);
			this.pictureBox1.SizeMode = System.Windows.Forms.PictureBoxSizeMode.StretchImage;
			this.pictureBox1.TabIndex = 0;
			this.pictureBox1.TabStop = false;
			// 
			// menu
			// 
			this.menu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																				 this.menuItem1,
																				 this.menuItem2,
																				 this.menuItem3,
																				 this.menuItem4,
																				 this.menuItem5,
																				 this.menuItem10,
																				 this.menuItem11});
			// 
			// menuItem1
			// 
			this.menuItem1.Index = 0;
			this.menuItem1.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					  this.menuItem12,
																					  this.menuItem13,
																					  this.menuItem14,
																					  this.menuItem15,
																					  this.menuItem16,
																					  this.menuItem17,
																					  this.menuItem18});
			this.menuItem1.Text = "File";
			// 
			// menuItem12
			// 
			this.menuItem12.Index = 0;
			this.menuItem12.Text = "Where";
			// 
			// menuItem13
			// 
			this.menuItem13.Index = 1;
			this.menuItem13.Text = "do";
			// 
			// menuItem14
			// 
			this.menuItem14.Index = 2;
			this.menuItem14.Text = "you";
			// 
			// menuItem15
			// 
			this.menuItem15.Index = 3;
			this.menuItem15.Text = "want";
			// 
			// menuItem16
			// 
			this.menuItem16.Index = 4;
			this.menuItem16.Text = "to";
			// 
			// menuItem17
			// 
			this.menuItem17.Index = 5;
			this.menuItem17.Text = "sleep";
			// 
			// menuItem18
			// 
			this.menuItem18.Index = 6;
			this.menuItem18.Text = "today?";
			// 
			// menuItem2
			// 
			this.menuItem2.Index = 1;
			this.menuItem2.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					  this.menuItem19,
																					  this.menuItem20,
																					  this.menuItem21,
																					  this.menuItem22,
																					  this.menuItem23});
			this.menuItem2.Text = "Edit";
			// 
			// menuItem19
			// 
			this.menuItem19.Index = 0;
			this.menuItem19.Text = "Dont";
			// 
			// menuItem20
			// 
			this.menuItem20.Index = 1;
			this.menuItem20.Text = "take";
			// 
			// menuItem21
			// 
			this.menuItem21.Index = 2;
			this.menuItem21.Text = "this";
			// 
			// menuItem22
			// 
			this.menuItem22.Index = 3;
			this.menuItem22.Text = "too";
			// 
			// menuItem23
			// 
			this.menuItem23.Index = 4;
			this.menuItem23.Text = "seriously";
			// 
			// menuItem3
			// 
			this.menuItem3.Index = 2;
			this.menuItem3.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					  this.menuItem24,
																					  this.menuItem25});
			this.menuItem3.Text = "View";
			// 
			// menuItem24
			// 
			this.menuItem24.Index = 0;
			this.menuItem24.Text = "got";
			// 
			// menuItem25
			// 
			this.menuItem25.Index = 1;
			this.menuItem25.Text = "balls?";
			// 
			// menuItem4
			// 
			this.menuItem4.Index = 3;
			this.menuItem4.Text = "There";
			// 
			// menuItem5
			// 
			this.menuItem5.Index = 4;
			this.menuItem5.Text = "Are";
			// 
			// menuItem10
			// 
			this.menuItem10.Index = 5;
			this.menuItem10.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					   this.menuItem28,
																					   this.menuItem29});
			this.menuItem10.Text = "Window";
			// 
			// menuItem28
			// 
			this.menuItem28.Index = 0;
			this.menuItem28.Text = "better";
			// 
			// menuItem29
			// 
			this.menuItem29.Index = 1;
			this.menuItem29.Text = "not";
			// 
			// menuItem11
			// 
			this.menuItem11.Index = 6;
			this.menuItem11.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					   this.menuItem26,
																					   this.menuItem27});
			this.menuItem11.Text = "Help";
			// 
			// menuItem26
			// 
			this.menuItem26.Index = 0;
			this.menuItem26.Text = "get";
			// 
			// menuItem27
			// 
			this.menuItem27.Checked = true;
			this.menuItem27.Index = 1;
			this.menuItem27.Text = "JBoss!";
			// 
			// toolBar1
			// 
			this.toolBar1.Appearance = System.Windows.Forms.ToolBarAppearance.Flat;
			this.toolBar1.AutoSize = false;
			this.toolBar1.Buttons.AddRange(new System.Windows.Forms.ToolBarButton[] {
																						this.toolBarButton1,
																						this.toolBarButton2,
																						this.toolBarButton3,
																						this.toolBarButton4,
																						this.toolBarButton5});
			this.toolBar1.DropDownArrows = true;
			this.toolBar1.ImageList = this.icons;
			this.toolBar1.Name = "toolBar1";
			this.toolBar1.ShowToolTips = true;
			this.toolBar1.Size = new System.Drawing.Size(568, 40);
			this.toolBar1.TabIndex = 3;
			// 
			// toolBarButton1
			// 
			this.toolBarButton1.ImageIndex = 0;
			this.toolBarButton1.Style = System.Windows.Forms.ToolBarButtonStyle.DropDownButton;
			// 
			// toolBarButton2
			// 
			this.toolBarButton2.ImageIndex = 1;
			// 
			// toolBarButton3
			// 
			this.toolBarButton3.ImageIndex = 2;
			// 
			// toolBarButton4
			// 
			this.toolBarButton4.ImageIndex = 3;
			this.toolBarButton4.Style = System.Windows.Forms.ToolBarButtonStyle.DropDownButton;
			// 
			// toolBarButton5
			// 
			this.toolBarButton5.ImageIndex = 4;
			this.toolBarButton5.Style = System.Windows.Forms.ToolBarButtonStyle.DropDownButton;
			// 
			// icons
			// 
			this.icons.ColorDepth = System.Windows.Forms.ColorDepth.Depth8Bit;
			this.icons.ImageSize = new System.Drawing.Size(16, 16);
			this.icons.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("icons.ImageStream")));
			this.icons.TransparentColor = System.Drawing.Color.Transparent;
			// 
			// statusBar1
			// 
			this.statusBar1.Anchor = ((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
				| System.Windows.Forms.AnchorStyles.Right);
			this.statusBar1.Dock = System.Windows.Forms.DockStyle.None;
			this.statusBar1.Location = new System.Drawing.Point(0, 360);
			this.statusBar1.Name = "statusBar1";
			this.statusBar1.Panels.AddRange(new System.Windows.Forms.StatusBarPanel[] {
																						  this.statusBarPanel1,
																						  this.statusBarPanel2,
																						  this.statusBarPanel3});
			this.statusBar1.ShowPanels = true;
			this.statusBar1.Size = new System.Drawing.Size(568, 24);
			this.statusBar1.TabIndex = 4;
			this.statusBar1.Text = "statusBar1";
			// 
			// statusBarPanel1
			// 
			this.statusBarPanel1.AutoSize = System.Windows.Forms.StatusBarPanelAutoSize.Spring;
			this.statusBarPanel1.Width = 352;
			// 
			// statusBarPanel2
			// 
			this.statusBarPanel2.Alignment = System.Windows.Forms.HorizontalAlignment.Center;
			// 
			// statusBarPanel3
			// 
			this.statusBarPanel3.Alignment = System.Windows.Forms.HorizontalAlignment.Right;
			// 
			// panel1
			// 
			this.panel1.Controls.AddRange(new System.Windows.Forms.Control[] {
																				 this.listView1,
																				 this.treeView1});
			this.panel1.Location = new System.Drawing.Point(0, 40);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(648, 320);
			this.panel1.TabIndex = 5;
			// 
			// listView1
			// 
			this.listView1.AllowColumnReorder = true;
			this.listView1.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
																						this.columnHeader1});
			this.listView1.ContextMenu = this.itemMenu;
			this.listView1.FullRowSelect = true;
			this.listView1.ImeMode = System.Windows.Forms.ImeMode.NoControl;
			this.listView1.LabelWrap = false;
			this.listView1.LargeImageList = this.icons;
			this.listView1.Location = new System.Drawing.Point(200, 0);
			this.listView1.Name = "listView1";
			this.listView1.Size = new System.Drawing.Size(368, 320);
			this.listView1.SmallImageList = this.icons;
			this.listView1.StateImageList = this.icons;
			this.listView1.TabIndex = 1;
			this.listView1.View = System.Windows.Forms.View.Details;
			// 
			// columnHeader1
			// 
			this.columnHeader1.Text = "No Selection Yet";
			this.columnHeader1.Width = 364;
			// 
			// itemMenu
			// 
			this.itemMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					 this.editItem});
			// 
			// editItem
			// 
			this.editItem.Index = 0;
			this.editItem.Text = "";
			this.editItem.Click += new System.EventHandler(this.Edit_Selected);
			// 
			// treeView1
			// 
			this.treeView1.ImageList = this.icons;
			this.treeView1.Name = "treeView1";
			this.treeView1.Nodes.AddRange(new System.Windows.Forms.TreeNode[] {
																				  new System.Windows.Forms.TreeNode("Store Services", 5, 5, new System.Windows.Forms.TreeNode[] {
																																													new System.Windows.Forms.TreeNode("Items", 6, 6),
																																													new System.Windows.Forms.TreeNode("BusinessPartners", 7, 7),
																																													new System.Windows.Forms.TreeNode("Orders", 8, 8)})});
			this.treeView1.Size = new System.Drawing.Size(200, 344);
			this.treeView1.TabIndex = 0;
			this.treeView1.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.Service_Selected);
			// 
			// comboBox1
			// 
			this.comboBox1.Location = new System.Drawing.Point(168, 3);
			this.comboBox1.Name = "comboBox1";
			this.comboBox1.Size = new System.Drawing.Size(247, 21);
			this.comboBox1.TabIndex = 6;
			this.comboBox1.Text = "http://localhost:8080/axis/services";
			// 
			// SampleForm
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(568, 385);
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.statusBar1,
																		  this.comboBox1,
																		  this.pictureBox1,
																		  this.toolBar1,
																		  this.panel1});
			this.Menu = this.menu;
			this.Name = "SampleForm";
			this.Text = "JBoss.net Interoperability Sample";
			this.Load += new System.EventHandler(this.SampleForm_Load);
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel1)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel2)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel3)).EndInit();
			this.panel1.ResumeLayout(false);
			this.ResumeLayout(false);

		}
		
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main() 
		{
			Application.Run(new SampleView());
		}

		#endregion

		/** initialize the form if necessary */
		private void SampleForm_Load(object sender, System.EventArgs e)
		{
		}

		
		/** switch the currently active controller */
		private void switchListController(ListController newController) 
		{
			// if the controller changes, we have to rearrange the listview
			if(!newController.Equals(currentController)) 
			{
				currentController=newController;
				editItem.Enabled=currentController.arrange();
			}
			// populate in each case
			currentController.populate();
		}

		
		/** reacts on the event that a treeView node has been selected */
		private void Service_Selected(Object sender, System.Windows.Forms.TreeViewEventArgs e) 
		{
			// if it is the items node
			if(e.Node.Text.Equals("Items")) 
			{
				// we switch to the item controller
				switchListController(itemController);
			} 
			else if(e.Node.Text.Equals("BusinessPartners")) 
			{
				// we switch to the business partner controller
				switchListController(businessPartnerController);
			}
		}

		
		/** reacts on the event that the new button has been pressed */
		private void Add_Item(Object sender, System.Windows.Forms.ToolBarButtonClickEventArgs args) 
		{
			if(args.Button.Equals(toolBarButton1) && currentController!=null) 
			{
				currentController.addNew();
			}
		}

		
		/** reacts on the event that the a key has been released */
		private void Key_Released(Object sender, System.Windows.Forms.KeyEventArgs args) 
		{
			// if it is the delete key, call the delete method with the current selection
			if(args.KeyCode.Equals(System.Windows.Forms.Keys.Delete) && currentController!=null) 
			{
				currentController.delete(listView1.SelectedItems);
			}
		}

		private void Edit_Selected(object sender, EventArgs e)
		{
			currentController.edit(listView1.SelectedItems);
		}

	} // SampleForm

} // namespace
