<%@ Page Language="C#" %>
<script runat="server">

protected void Page_Load(object sender, EventArgs e) {

	AuthMethod.Text = "Anonymous";
	AuthUser.Text = "none";
	AuthType.Text = "unknown";
	ImpersonationLevel.Text = "unknown";

	string AUTH_TYPE=Request.ServerVariables["AUTH_TYPE"];
	if (AUTH_TYPE.Length > 0)
		AuthMethod.Text = AUTH_TYPE;

	string AUTH_USER=Request.ServerVariables["AUTH_USER"];
	if (AUTH_USER.Length > 0)
		AuthUser.Text  = AUTH_USER;

	AuthType.Text = System.Threading.Thread.CurrentPrincipal.Identity.AuthenticationType.ToString();

	System.Security.Principal.WindowsIdentity winId = (System.Security.Principal.WindowsIdentity)System.Threading.Thread.CurrentPrincipal.Identity;
	ImpersonationLevel.Text = winId.ImpersonationLevel.ToString();
}

</script>

AuthenticationMethod: <asp:literal id=AuthMethod runat=server/>
Identity: <asp:literal id=AuthUser runat=server />
AuthType: <asp:literal id=AuthType runat=server />
ImpersonationLevel: <asp:literal id=ImpersonationLevel runat=server />
