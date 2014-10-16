Function GetSOAPObject() As Object
  Static SOAPObject As Object
  If SOAPObject Is Nothing Then
    Set SOAPObject = CreateObject("SOAP.Lite").new
    If SOAPObject Is Nothing Then
      MsgBox ("Oops, no SOAP.Lite on this machine")
      Exit Function
    End If
    SOAPObject.proxy("http://soaplite:authtest@services.soaplite.com/auth/examples.cgi").uri ("http://www.soaplite.com/My/Examples")
  End If
  Set GetSOAPObject = SOAPObject
End Function

Sub GetStateNameRemotely()
  Application.StatusBar = "Running SOAP call..."
  Range("Current").FormulaR1C1 = GetSOAPObject.getStateName(Sheet1.TextBox1.Value).result
  Application.StatusBar = False
End Sub
