require ["imap4flags"];

if header :contains "X-Set-Seen" ["Yes", "YES", "1"] {
	setflag "\\Seen";
}

keep;
