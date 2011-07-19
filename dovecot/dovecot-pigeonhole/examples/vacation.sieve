require ["fileinto","reject", "vacation"];
if allof (header :contains  "X-Spam-Flag" "YES") 
{
    discard ;
}

elsif allof (header :contains "subject" "<quation>") 
{
vacation 
:addresses "<name@domain.ru>"
:subject "<Answear>" 
:mime "MIME-Version: 1.0
Content-Type: text/html; charset=KOI8-R
Content-Transfer-Encoding: 7bit
<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">
<HTML><HEAD><META http-equiv=Content-Type content=\"text/html; charset=windows-KOI8-R\">
</HEAD><BODY>123</BODY></HTML>";
 discard ;
}
else 
{
     keep;
}
