require ["fileinto"];

if header :is "Sender" "owner-ietf-mta-filters@imc.example.com"
{
	fileinto "lists.sieve";
}
elsif header :is "Sender" "owner-ietf-imapext@imc.example.com"
{
	fileinto "lists.imapext";
}
