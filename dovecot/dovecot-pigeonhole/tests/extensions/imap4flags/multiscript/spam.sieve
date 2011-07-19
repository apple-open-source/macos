require ["fileinto"];

if header :contains "X-Spam-Flag" ["Yes", "YES", "1"] {
  fileinto "SPAM";
}
keep;


