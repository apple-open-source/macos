require ["fileinto", "variables", "envelope"];

if header :contains "X-Group-Mail" ["Yes", "YES", "1"] {
	if header :contains "X-Spam-Flag" ["Yes", "YES", "1"] {
		if envelope :matches :localpart "to" "*" {
			fileinto "group/${1}/SPAM"; stop;
		}
	}
	if address :is ["To"] "sales@florist.ru" {
		fileinto "group/info/Orders";
	}
	stop;
}
keep;
