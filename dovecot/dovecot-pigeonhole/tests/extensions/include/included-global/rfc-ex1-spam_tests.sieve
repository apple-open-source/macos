require ["reject"];

if anyof (header :contains "Subject" "$$",
	header :contains "Subject" "Make money")
{
	reject "Not wanted";
}
