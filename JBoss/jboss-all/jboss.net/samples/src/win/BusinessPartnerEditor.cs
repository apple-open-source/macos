using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using JBoss_Net_Sample.businessPartner;

namespace org.jboss.net.samples.store
{
	/// <summary>
	/// Summary description for BusinessPartnerEditor.
	/// </summary>
	public class BusinessPartnerEditor : System.Windows.Forms.Form
	{
		private System.Windows.Forms.Button button1;
		private System.Windows.Forms.Button button2;
		private System.Windows.Forms.Label nameLabel;
		private System.Windows.Forms.TextBox cityBox;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox textBox1;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.ComboBox stateBox;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		BusinessPartner bp;
		ListController bpc;

		public BusinessPartnerEditor(BusinessPartner bp, ListController bpc)
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();
			this.bp=bp;
			this.bpc=bpc;
			stateBox.Items.Add(StateType.IN.ToString());
			stateBox.Items.Add(StateType.OH.ToString());
			stateBox.Items.Add(StateType.TX.ToString());
			
			nameLabel.Text=bp.name;
			if(bp.address!=null) 
			{
				cityBox.Text=bp.address.city;
				if(bp.address.phoneNumber!=null) 
				{
					textBox1.Text=bp.address.phoneNumber.number;
				}
				stateBox.Text=bp.address.state.ToString();
			}

			//
			// TODO: Add any constructor code after InitializeComponent call
			//
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if(components != null)
				{
					components.Dispose();
				}
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			System.Resources.ResourceManager resources = new System.Resources.ResourceManager(typeof(BusinessPartnerEditor));
			this.button1 = new System.Windows.Forms.Button();
			this.button2 = new System.Windows.Forms.Button();
			this.nameLabel = new System.Windows.Forms.Label();
			this.cityBox = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.textBox1 = new System.Windows.Forms.TextBox();
			this.label3 = new System.Windows.Forms.Label();
			this.stateBox = new System.Windows.Forms.ComboBox();
			this.SuspendLayout();
			// 
			// button1
			// 
			this.button1.Location = new System.Drawing.Point(0, 248);
			this.button1.Name = "button1";
			this.button1.Size = new System.Drawing.Size(184, 24);
			this.button1.TabIndex = 0;
			this.button1.Text = "ok";
			this.button1.Click += new System.EventHandler(this.button1_Click);
			// 
			// button2
			// 
			this.button2.Location = new System.Drawing.Point(192, 248);
			this.button2.Name = "button2";
			this.button2.Size = new System.Drawing.Size(96, 24);
			this.button2.TabIndex = 1;
			this.button2.Text = "cancel";
			this.button2.Click += new System.EventHandler(this.button2_Click);
			// 
			// nameLabel
			// 
			this.nameLabel.BackColor = System.Drawing.SystemColors.Control;
			this.nameLabel.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.nameLabel.Image = ((System.Drawing.Bitmap)(resources.GetObject("nameLabel.Image")));
			this.nameLabel.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
			this.nameLabel.Location = new System.Drawing.Point(0, 8);
			this.nameLabel.Name = "nameLabel";
			this.nameLabel.Size = new System.Drawing.Size(288, 24);
			this.nameLabel.TabIndex = 2;
			this.nameLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// cityBox
			// 
			this.cityBox.ImeMode = System.Windows.Forms.ImeMode.AlphaFull;
			this.cityBox.Location = new System.Drawing.Point(48, 40);
			this.cityBox.Name = "cityBox";
			this.cityBox.Size = new System.Drawing.Size(240, 20);
			this.cityBox.TabIndex = 3;
			this.cityBox.Text = "";
			// 
			// label1
			// 
			this.label1.Location = new System.Drawing.Point(3, 42);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(40, 16);
			this.label1.TabIndex = 4;
			this.label1.Text = "City";
			this.label1.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// label2
			// 
			this.label2.Location = new System.Drawing.Point(3, 64);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(40, 16);
			this.label2.TabIndex = 5;
			this.label2.Text = "Phone";
			this.label2.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// textBox1
			// 
			this.textBox1.ImeMode = System.Windows.Forms.ImeMode.Alpha;
			this.textBox1.Location = new System.Drawing.Point(48, 64);
			this.textBox1.Name = "textBox1";
			this.textBox1.Size = new System.Drawing.Size(240, 20);
			this.textBox1.TabIndex = 6;
			this.textBox1.Text = "";
			// 
			// label3
			// 
			this.label3.Location = new System.Drawing.Point(4, 91);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(40, 16);
			this.label3.TabIndex = 7;
			this.label3.Text = "State";
			this.label3.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// stateBox
			// 
			this.stateBox.Location = new System.Drawing.Point(48, 88);
			this.stateBox.Name = "stateBox";
			this.stateBox.Size = new System.Drawing.Size(240, 21);
			this.stateBox.TabIndex = 8;
			// 
			// BusinessPartnerEditor
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(292, 273);
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.stateBox,
																		  this.label3,
																		  this.textBox1,
																		  this.label2,
																		  this.label1,
																		  this.cityBox,
																		  this.nameLabel,
																		  this.button2,
																		  this.button1});
			this.Name = "BusinessPartnerEditor";
			this.Text = "Edit BusinessPartner";
			this.ResumeLayout(false);

		}
		#endregion

		private void button1_Click(object sender, System.EventArgs e)
		{
			if(bp.address==null) 
			{
				bp.address=new Address();
			}

			if(bp.address.phoneNumber==null) 
			{
				bp.address.phoneNumber=new Phone();
			}

			bp.address.city=cityBox.Text;
			if(stateBox.Text.Equals(StateType.IN.ToString()))
			{
				bp.address.state=StateType.IN;
			} else if(stateBox.Text.Equals(StateType.OH.ToString())) 
			{
				bp.address.state=StateType.OH;
			} else if(stateBox.Text.Equals(StateType.TX.ToString())) 
			{
				bp.address.state=StateType.TX;
			} 
			bp.address.phoneNumber.number=textBox1.Text;

			bpc.processUpdate(bp);

			this.Close();
		}

		private void button2_Click(object sender, System.EventArgs e)
		{
			this.Close();
		}
	}
}
