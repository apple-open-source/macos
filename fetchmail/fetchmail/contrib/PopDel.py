# PopDel.py  Copyright Richard Harris 2002
#
# author: Richard Harris (rover@emptydog.com)
# The author releases this file under the GPL
# license. See COPYING for specifics.
#
# See PopDel.manual for the use of this Python class.
#
# created: 01 May 2002
# last modified: 27 Apr 2006
#
# change log:
# Matthias Andree, April 2006:
#	Bump version to 0.1+jc2 and mark Joshua Crawford
#	as additional author.
#	Reformat ESR's change log entry
#	Note: emptydog.com is currently not registered.
# Joshua Crawford, April 2006:
#	Display From: address.
#	List every email, even if it has no Subject: header.
#	  this also avoids indexing errors (that caused
#	  deleting the wrong message)
# Joshua Crawford, November 2004:
# 	Out of range error fixed.
#	Allow for all caps "SUBJECT:".
#	Display user and server name in messages.
#	Don't prompt for save if no changes.
#	Don't clear the screen until we're displaying a menu.
#	Check for invalid choice.
#	Check all arguments exist.
#	Check for errors in POP.
#	Return 1 on errors, 0 otherwise.
# Eric S. Raymond, January 2003:
#	Hacked to support message ranges.
#
import os, poplib, re, string, sys

class PopDel:
	HDR = "\nPopDel - Delete messages from popmail - Ver. 0.1+jc2"
	BYE = "\n  PopDel Ver.0.1+jc2 by Richard Harris and Joshua Crawford\n" +\
#		  "     site - http://emptydog.com/\n" +\
		  "     email - Richard Harris <rover@emptydog.com>\n" +\
		  "     email - Joshua Crawford <jgcrawford@gmail.com>\n"
	PROMPT1 = "Choose message number to delete or 'q' to quit: "
	PROMPT2 = "Quit or abort: "
	CHOICES = ["Save changes and quit.",
			   "Abort and make no deletions."]
	
	def __init__(self):
		self.done = 0
		self.dirty = 0
		return

	# get user to choose an element from thing
	def query(self, thing, prompt):
		length = len(thing)
		choice = [length+1]
		for i in range(0, length):
			print '(' + `i +  1` + ') ' + thing[i]
		while filter(lambda x: x > length, choice):
			choice = raw_input(prompt)
			if (choice == 'q'):
				self.done = 1
				choice = [-1]
			else:
				try:
					choice = map(int, string.split(choice, "-"))
				except:
					choice = [length + 1]
				if len(choice) > 1:
					choice = range(choice[0], choice[1]+1)
		return choice

	def run(self):
		#log in
		print self.HDR

		subjects = []

		if (len(sys.argv) < 4):
			print 'Usage: ' + sys.argv[0] + ' pop3.host.name username password'
			return 1

		try:
			M = poplib.POP3(sys.argv[1])
		except:
			print 'Could not reach ' + sys.argv[1]
			return 1
		try:
			M.user(sys.argv[2])
		except:
			print 'Bad username ' + sys.argv[2] + '@' + sys.argv[1]
			M.quit()
			return 1
		try:
			M.pass_(sys.argv[3])
		except:
			print 'Bad password for ' + sys.argv[2] + '@' + sys.argv[1]
			M.quit()
			return 1
#		M.set_debuglevel(1)
		try:
			messages = M.list()
		except:
			print 'Error reading listing for ' + sys.argv[2] + '@' + sys.argv[1]
			M.quit()
			return 1

		list = messages[1]
		if (len(list) == 0):
			M.quit()
			print '\nNo messages for ' + sys.argv[2] + '@' + sys.argv[1]
		else:
			for entry in list:
				tokens = string.split(entry)
				subject = '(no subject)'
				address = '(no address)'
				try:
					head = M.top(int(tokens[0]), 32)
				except:
					print 'Error issuing TOP command for ' + sys.argv[2] + '@' + sys.argv[1]
					if self.dirty:
						M.rset()
					M.quit()
					return 1
				for line in head[1]:
					if (string.find(string.upper(line), 'SUBJECT:') == 0):
						subject = line[9:]
					if (string.find(string.upper(line), 'FROM:') == 0):
						line = line[6:]
						result = re.search(r'([^\t <>]*@[^\t <>]*)', line)
						if (result != None):
							address = result.expand(r'\1')
				subj = address[:40] + ' [' + tokens[1] + 'o] ' + subject
				subjects.append(subj)

			while not self.done:
				os.system('clear')
				print self.HDR
				print '\nMessages for ' + sys.argv[2] + '@' + sys.argv[1] + ':'
				msglist = self.query(subjects, self.PROMPT1)
				print "Choice:", msglist
				for msg in msglist:
					if (msg > 0):
						try:
							M.dele(msg)
						except:
							print 'Error deleting message #' + `msg`
							if self.dirty:
								M.rset()
							M.quit()
							return 1
						self.dirty = 1
						subjects[msg-1] = subjects[msg-1] + ' -X-'

			if not self.dirty:
				M.quit()
			else:
				print '\nExit Options:'
				choice = self.query(self.CHOICES, self.PROMPT2)
				print "Choice:", choice
				if (choice == [1]):		# commit changes and quit
					M.quit()
				else:				# reset and quit
					M.rset()
					M.quit()

		print self.BYE
		return

#-----------------main
obj = PopDel()
sys.exit(obj.run())
