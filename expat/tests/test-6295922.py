from xml.parsers import expat

xml = "<?xml version='1.0' encoding='iso8859'?><s>%s</s>" % ('a' * 1025)

def handler(text):
	raise Exception

parser = expat.ParserCreate()
parser.CharacterDataHandler = handler
parser.Parse(xml)
