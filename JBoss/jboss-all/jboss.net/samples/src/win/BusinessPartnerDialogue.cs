using System;
using System.Windows;
using System.Windows.Forms;

namespace org.jboss.net.samples.store
{
	/// <summary>
	/// Summary description for ItemDialogue.
	/// </summary>
	public class BusinessPartnerDialogue : System.Windows.Forms.Form
	{
		private TextBox textBox1;
		private System.Windows.Forms.Button button1;
		private System.Windows.Forms.Button button2;
		private bool ok=false;
		
		public BusinessPartnerDialogue()
		{
			InitializeComponent();
		}

		public string getText() 
		{
			return textBox1.Text;
		}

		public bool getOk() 
		{
			return ok;
		}
		
		private void InitializeComponent()
		{
			this.textBox1 = new System.Windows.Forms.TextBox();
			this.button1 = new System.Windows.Forms.Button();
			this.button2 = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// textBox1
			// 
			this.textBox1.Name = "textBox1";
			this.textBox1.Size = new System.Drawing.Size(296, 20);
			this.textBox1.TabIndex = 0;
			this.textBox1.Text = "New BusinessPartner";
			// 
			// button1
			// 
			this.button1.Location = new System.Drawing.Point(0, 24);
			this.button1.Name = "button1";
			this.button1.Size = new System.Drawing.Size(168, 24);
			this.button1.TabIndex = 1;
			this.button1.Text = "ok";
			this.button1.Click += new System.EventHandler(this.button1_Click);
			// 
			// button2
			// 
			this.button2.Location = new System.Drawing.Point(176, 24);
			this.button2.Name = "button2";
			this.button2.Size = new System.Drawing.Size(120, 24);
			this.button2.TabIndex = 2;
			this.button2.Text = "cancel";
			this.button2.Click += new System.EventHandler(this.button2_Click);
			// 
			// BusinessPartnerDialogue
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(296, 53);
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.button2,
																		  this.button1,
																		  this.textBox1});
			this.Name = "BusinessPartnerDialogue";
			this.Text = "Please name the new businesspartner";
			this.Load += new System.EventHandler(this.BusinessPartnerDialogue_Load);
			this.ResumeLayout(false);

		}

		private void button1_Click(object sender, System.EventArgs e)
		{
			ok=true;
			this.Close();
		}

		private void button2_Click(object sender, System.EventArgs e)
		{
			ok=false;
			this.Close();
		}

		private void BusinessPartnerDialogue_Load(object sender, System.EventArgs e)
		{
		
		}

	}
}
