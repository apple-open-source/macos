require ["reject"];

if header :contains "Subject" "XXXX"
{
	reject "Not wanted";
}
elsif header :is "From" "money@example.com"
{
	reject "Not wanted";
}
