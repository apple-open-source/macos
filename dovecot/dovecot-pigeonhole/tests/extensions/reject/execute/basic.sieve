require "reject";

if address :contains "to" "frop.example" {
	reject "Don't send unrequested messages.";
	stop;
}

keep;
