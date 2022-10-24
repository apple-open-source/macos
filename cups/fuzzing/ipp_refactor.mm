#include <Foundation/Foundation.h>

#include "config.h"
#include "ipp.h"
#include "ipp-private.h"

#define DEBUG_printf(xx)	/**/
#define DEBUG_puts(ss)		/**/

extern "C" ipp_state_t                /* O - Current state */
ippWriteIO2(void           *dst,        /* I - Destination */
			ipp_iocb_t cb,        /* I - Write callback function */
			ipp_t          *parent,        /* I - Parent IPP message */
			ipp_t          *ipp);        /* I - IPP data */

/****** refactoring ****/

struct IPPIOWriter {
	IPPIOWriter(ipp_iocb_t cb, void* dt) {
		_cb = cb;
		_dt = dt;
		_buffer = (uint8_t*) malloc(IPP_BUF_SIZE);
		_p = _buffer;
		_pEnd = &_buffer[IPP_BUF_SIZE];
	}
	~IPPIOWriter() {
		flush();
		free((void*) _buffer);
	}

	void addOctets(const UInt8* base, size_t len) {
		withBufferN(len, ^(UInt8* p) {
			memcpy(p, base, len);
		});
	}

	// REMINDSMA: I would't ordinarily have done this
	// but it made *dstptr++ much easier to repalce.  I'm sorry.
	void operator<<(const UInt8& val) {
		withBufferN(sizeof(UInt8), ^(UInt8* p) {
			*p = val;
		});
	}

private:
	void withBufferN(size_t len, void (^cb)(UInt8*)) {
		if (&_p[len] >= _pEnd) {
			flush();

			if (&_p[len] >= _pEnd) {
				// our buffer was never big enough
				free((void*) _buffer);
				_buffer = (UInt8*) malloc(len);
				_p = _buffer;
				_pEnd = &_buffer[len];
			}
		}

		UInt8* pNow = _p;
		_p = &_p[len];
		cb(pNow);
	}

	void flush() {
		ptrdiff_t len = _p - _buffer;
		if (len > 0) {
			ssize_t ctWrote = (*_cb)(_dt, _buffer, len);
			assert(ctWrote > 0);
			_p = _buffer;
		}
	}

private:
	void* _dt;
	ipp_iocb_t _cb;
	UInt8* _buffer;
	UInt8* _p;
	UInt8* _pEnd;
};

static void ippWriteWithWriter(IPPIOWriter& writer, ipp_t* parent, ipp_t* ipp);

static void writeHeader(IPPIOWriter& writer, ipp_t *ipp, ipp_t *parent)
{
	if (parent == NULL)
	{
		/*
		 * Send the request header:
		 *
		 *                 Version = 2 bytes
		 *  Operation/Status Code = 2 bytes
		 *             Request ID = 4 bytes
		 *                  Total = 8 bytes
		 */

		writer << ipp->request.any.version[0];
		writer << ipp->request.any.version[1];
		writer << (ipp_uchar_t)(ipp->request.any.op_status >> 8);
		writer << (ipp_uchar_t)ipp->request.any.op_status;
		writer << (ipp_uchar_t)(ipp->request.any.request_id >> 24);
		writer << (ipp_uchar_t)(ipp->request.any.request_id >> 16);
		writer << (ipp_uchar_t)(ipp->request.any.request_id >> 8);
		writer << (ipp_uchar_t)ipp->request.any.request_id;

		DEBUG_printf(("2ippWriteIO: version=%d.%d", buffer[0], buffer[1]));
		DEBUG_printf(("2ippWriteIO: op_status=%04x", ipp->request.any.op_status));
		DEBUG_printf(("2ippWriteIO: request_id=%d", ipp->request.any.request_id));
	}

	/*
	 * Reset the state engine to point to the first attribute
	 * in the request/response, with no current group.
	 */

	ipp->state   = IPP_STATE_ATTRIBUTE;
	ipp->current = ipp->attrs;
	ipp->curtag = IPP_TAG_ZERO;

	DEBUG_printf(("1ippWriteIO: ipp->current=%p", ipp->current));
}

static void writeIntegersOrEnums(IPPIOWriter& writer, ipp_attribute_t* attr)
{
	int i;
	_ipp_value_t* value;

	for (i = 0, value = attr->values; i < attr->num_values; i ++, value ++)
	{
		if (i)
		{
			/*
			 * Arrays and sets are done by sending additional
			 * values with a zero-length name...
			 */

			writer << (ipp_uchar_t)attr->value_tag;
			writer << 0;
			writer << 0;
		}

		/*
		 * Integers and enumerations are both 4-byte signed
		 * (twos-complement) values.
		 *
		 * Put the 2-byte length and 4-byte value into the buffer...
		 */

		writer << 0;
		writer << 4;
		writer << (ipp_uchar_t)(value->integer >> 24);
		writer << (ipp_uchar_t)(value->integer >> 16);
		writer << (ipp_uchar_t)(value->integer >> 8);
		writer << (ipp_uchar_t)value->integer;
	}
}

static void writeBooleans(IPPIOWriter& writer, ipp_attribute_t* attr)
{
	int i;
	_ipp_value_t* value;

	for (i = 0, value = attr->values; i < attr->num_values; i ++, value ++)
	{
		if (i)
		{
			/*
			 * Arrays and sets are done by sending additional
			 * values with a zero-length name...
			 */

			writer << (ipp_uchar_t)attr->value_tag;
			writer << 0;
			writer << 0;
		}

		/*
		 * Boolean values are 1-byte; 0 = false, 1 = true.
		 *
		 * Put the 2-byte length and 1-byte value into the buffer...
		 */

		writer << 0;
		writer << 1;
		writer << (ipp_uchar_t)value->boolean;
	}
}

static void writeStrings(IPPIOWriter& writer, ipp_attribute_t* attr)
{
	int i;
	int n;
	_ipp_value_t* value;

	for (i = 0, value = attr->values; i < attr->num_values; i ++, value ++)
	{
		if (i)
		{
			/*
			 * Arrays and sets are done by sending additional
			 * values with a zero-length name...
			 */

			DEBUG_printf(("2ippWriteIO: writing value tag=%x(%s)", attr->value_tag, ippTagString(attr->value_tag)));
			DEBUG_printf(("2ippWriteIO: writing name=0,\"\""));

			writer << (ipp_uchar_t)attr->value_tag;
			writer << 0;
			writer << 0;
		}

		if (value->string.text != NULL)
			n = (int)strlen(value->string.text);
		else
			n = 0;

		if (n > (IPP_BUF_SIZE - 2))
		{
			@throw [NSException exceptionWithName:@"IPPInternal" reason:@"'text' value length too large" userInfo:nil];
		}

		DEBUG_printf(("2ippWriteIO: writing string=%d,\"%s\"", n, value->string.text));

		/*
		 * All simple strings consist of the 2-byte length and
		 * character data without the trailing nul normally found
		 * in C strings.   Also, strings cannot be longer than IPP_MAX_LENGTH
		 * bytes since the 2-byte length is a signed (twos-complement)
		 * value.
		 *
		 * Put the 2-byte length and string characters in the buffer.
		 */

		writer << (ipp_uchar_t)(n >> 8);
		writer << (ipp_uchar_t)n;

		if (n > 0)
		{
			writer.addOctets((const UInt8*) value->string.text, (size_t)n);
		}
	}
}

static void writeDates(IPPIOWriter& writer, ipp_attribute_t* attr)
{
	int i;
	_ipp_value_t* value;

	for (i = 0, value = attr->values; i < attr->num_values; i ++, value ++)
	{
		if (i)
		{
			/*
			 * Arrays and sets are done by sending additional
			 * values with a zero-length name...
			 */

			writer << (ipp_uchar_t)attr->value_tag;
			writer << 0;
			writer << 0;
		}

		/*
		 * Date values consist of a 2-byte length and an
		 * 11-byte date/time structure defined by RFC 1903.
		 *
		 * Put the 2-byte length and 11-byte date/time
		 * structure in the buffer.
		 */

		writer << 0;
		writer << 11;
		writer.addOctets(value->date, (size_t)11);
	}
}

static void writeResolutions(IPPIOWriter& writer, ipp_attribute_t* attr)
{
	int i;
	_ipp_value_t* value;

	for (i = 0, value = attr->values; i < attr->num_values; i ++, value ++)
	{
		if (i)
		{
			/*
			 * Arrays and sets are done by sending additional
			 * values with a zero-length name...
			 */

			writer << (ipp_uchar_t)attr->value_tag;
			writer << 0;
			writer << 0;
		}

		/*
		 * Resolution values consist of a 2-byte length, * 4-byte horizontal resolution value, 4-byte vertical
		 * resolution value, and a 1-byte units value.
		 *
		 * Put the 2-byte length and resolution value data
		 * into the buffer.
		 */

		writer << 0;
		writer << 9;
		writer << (ipp_uchar_t)(value->resolution.xres >> 24);
		writer << (ipp_uchar_t)(value->resolution.xres >> 16);
		writer << (ipp_uchar_t)(value->resolution.xres >> 8);
		writer << (ipp_uchar_t)value->resolution.xres;
		writer << (ipp_uchar_t)(value->resolution.yres >> 24);
		writer << (ipp_uchar_t)(value->resolution.yres >> 16);
		writer << (ipp_uchar_t)(value->resolution.yres >> 8);
		writer << (ipp_uchar_t)value->resolution.yres;
		writer << (ipp_uchar_t)value->resolution.units;
	}
}

static void writeRanges(IPPIOWriter& writer, ipp_attribute_t* attr)
{
	int i;
	_ipp_value_t* value;

	for (i = 0, value = attr->values; i < attr->num_values; i ++, value ++)
	{
		if (i)
		{
			/*
			 * Arrays and sets are done by sending additional
			 * values with a zero-length name...
			 */

			writer << (ipp_uchar_t)attr->value_tag;
			writer << 0;
			writer << 0;
		}

		/*
		 * Range values consist of a 2-byte length, * 4-byte lower value, and 4-byte upper value.
		 *
		 * Put the 2-byte length and range value data
		 * into the buffer.
		 */

		writer << 0;
		writer << 8;
		writer << (ipp_uchar_t)(value->range.lower >> 24);
		writer << (ipp_uchar_t)(value->range.lower >> 16);
		writer << (ipp_uchar_t)(value->range.lower >> 8);
		writer << (ipp_uchar_t)value->range.lower;
		writer << (ipp_uchar_t)(value->range.upper >> 24);
		writer << (ipp_uchar_t)(value->range.upper >> 16);
		writer << (ipp_uchar_t)(value->range.upper >> 8);
		writer << (ipp_uchar_t)value->range.upper;
	}
}

static void writeTextLangs(IPPIOWriter& writer, ipp_attribute_t* attr)
{
	int i;
	int n;
	_ipp_value_t* value;

	for (i = 0, value = attr->values; i < attr->num_values; i ++, value ++)
	{
		if (i)
		{
			/*
			 * Arrays and sets are done by sending additional
			 * values with a zero-length name...
			 */

			writer << (ipp_uchar_t)attr->value_tag;
			writer << 0;
			writer << 0;
		}

		/*
		 * textWithLanguage and nameWithLanguage values consist
		 * of a 2-byte length for both strings and their
		 * individual lengths, a 2-byte length for the
		 * character string, the character string without the
		 * trailing nul, a 2-byte length for the character
		 * set string, and the character set string without
		 * the trailing nul.
		 */

		n = 4;

		if (value->string.language != NULL)
			n += (int)strlen(value->string.language);

		if (value->string.text != NULL)
			n += (int)strlen(value->string.text);

		if (n > (IPP_BUF_SIZE - 2))
		{
			@throw [NSException exceptionWithName:@"IPPInternal" reason:@"'text' value length too large" userInfo:nil];
		}

		/* Length of entire value */
		writer << (ipp_uchar_t)(n >> 8);
		writer << (ipp_uchar_t)n;

		/* Length of language */
		if (value->string.language != NULL)
			n = (int)strlen(value->string.language);
		else
			n = 0;

		writer << (ipp_uchar_t)(n >> 8);
		writer << (ipp_uchar_t)n;

		/* Language */
		if (n > 0)
		{
			writer.addOctets((const UInt8*) value->string.language, (size_t)n);
		}

		/* Length of text */
		if (value->string.text != NULL)
			n = (int)strlen(value->string.text);
		else
			n = 0;

		writer << (ipp_uchar_t)(n >> 8);
		writer << (ipp_uchar_t)n;

		/* Text */
		if (n > 0)
		{
			writer.addOctets((const UInt8*) value->string.text, (size_t)n);
		}
	}
}

static void writeBeginCollection(IPPIOWriter& writer, ipp_attribute_t* attr, ipp_t *ipp)
{
	int i;
	_ipp_value_t* value;
	for (i = 0, value = attr->values; i < attr->num_values; i ++, value ++)
	{
		/*
		 * Collections are written with the begin-collection
		 * tag first with a value of 0 length, followed by the
		 * attributes in the collection, then the end-collection
		 * value...
		 */

		if (i)
		{
			/*
			 * Arrays and sets are done by sending additional
			 * values with a zero-length name...
			 */

			writer << (ipp_uchar_t)attr->value_tag;
			writer << 0;
			writer << 0;
		}

		/*
		 * Write a data length of 0 and flush the buffer...
		 */

		writer << 0;
		writer << 0;

		/*
		 * Then write the collection attribute...
		 */

		value->collection->state = IPP_STATE_IDLE;

		if (value->collection == NULL) {
			@throw [NSException exceptionWithName:@"IPPInternal" reason:@"IPP Value nil" userInfo:nil];
		}

		ippWriteWithWriter(writer, ipp, value->collection);
	}
}

static void writeDefaultUnknown(IPPIOWriter& writer, ipp_attribute_t* attr)
{
	int i;
	int n;
	_ipp_value_t* value;

	for (i = 0, value = attr->values; i < attr->num_values; i ++, value ++)
	{
		if (i)
		{
			/*
			 * Arrays and sets are done by sending additional
			 * values with a zero-length name...
			 */

			writer << (ipp_uchar_t)attr->value_tag;
			writer << 0;
			writer << 0;
		}

		/*
		 * An unknown value might some new value that a
		 * vendor has come up with. It consists of a
		 * 2-byte length and the bytes in the unknown
		 * value buffer.
		 */

		n = value->unknown.length;

		if (n > (IPP_BUF_SIZE - 2))
		{
			@throw [NSException exceptionWithName:@"IPPInternal" reason:@"'unknown' value length too large" userInfo:nil];
		}

		/* Length of unknown value */
		writer << (ipp_uchar_t)(n >> 8);
		writer << (ipp_uchar_t)n;

		/* Value */
		if (n > 0)
		{
			writer.addOctets((const UInt8*) value->unknown.data, (size_t)n);
		}
	}
}

static void writeAllCurrentAttributes(IPPIOWriter& writer, ipp_t *ipp, ipp_t *parent)
{
	int n;
	ipp_attribute_t* attr;

	while (ipp->current != NULL)
	{
		/*
		 * Write this attribute...
		 */

		attr   = ipp->current;

		ipp->current = ipp->current->next;

		if (!parent)
		{
			if (ipp->curtag != attr->group_tag)
			{
				/*
				 * Send a group tag byte...
				 */

				ipp->curtag = attr->group_tag;

				if (attr->group_tag == IPP_TAG_ZERO)
					continue;

				DEBUG_printf(("2ippWriteIO: wrote group tag=%x(%s)", attr->group_tag, ippTagString(attr->group_tag)));
				writer << (ipp_uchar_t)attr->group_tag;
			}
			else if (attr->group_tag == IPP_TAG_ZERO)
				continue;
		}

		DEBUG_printf(("1ippWriteIO: %s (%s%s)", attr->name, attr->num_values > 1 ? "1setOf " : "", ippTagString(attr->value_tag)));

		/*
		 * Write the attribute tag and name.
		 *
		 * The attribute name length does not include the trailing nul
		 * character in the source string.
		 *
		 * Collection values (parent != NULL) are written differently...
		 */

		if (parent == NULL)
		{
			/*
			 * Get the length of the attribute name, and make sure it won't
			 * overflow the buffer...
			 */

			if ((n = (int)strlen(attr->name)) > (IPP_BUF_SIZE - 8))
			{
				@throw [NSException exceptionWithName:@"IPPInternal" reason:@"'attr name' value length too large" userInfo:nil];
			}

			/*
			 * Write the value tag, name length, and name string...
			 */

			DEBUG_printf(("2ippWriteIO: writing value tag=%x(%s)", attr->value_tag, ippTagString(attr->value_tag)));
			DEBUG_printf(("2ippWriteIO: writing name=%d,\"%s\"", n, attr->name));

			if (attr->value_tag > 0xff)
			{
				writer << IPP_TAG_EXTENSION;
				writer << (ipp_uchar_t)(attr->value_tag >> 24);
				writer << (ipp_uchar_t)(attr->value_tag >> 16);
				writer << (ipp_uchar_t)(attr->value_tag >> 8);
				writer << (ipp_uchar_t)attr->value_tag;
			}
			else
				writer << (ipp_uchar_t)attr->value_tag;

			writer << (ipp_uchar_t)(n >> 8);
			writer << (ipp_uchar_t)n;
			writer.addOctets((const UInt8*) attr->name, (size_t)n);
		}
		else
		{
			/*
			 * Get the length of the attribute name, and make sure it won't
			 * overflow the buffer...
			 */

			if ((n = (int)strlen(attr->name)) > (IPP_BUF_SIZE - 12))
			{
				@throw [NSException exceptionWithName:@"IPPInternal" reason:@"'attr->name' value length too large" userInfo:nil];
			}

			/*
			 * Write the member name tag, name length, name string, value tag, * and empty name for the collection member attribute...
			 */

			DEBUG_printf(("2ippWriteIO: writing value tag=%x(memberName)", IPP_TAG_MEMBERNAME));
			DEBUG_printf(("2ippWriteIO: writing name=%d,\"%s\"", n, attr->name));
			DEBUG_printf(("2ippWriteIO: writing value tag=%x(%s)", attr->value_tag, ippTagString(attr->value_tag)));
			DEBUG_puts("2ippWriteIO: writing name=0,\"\"");

			writer << IPP_TAG_MEMBERNAME;
			writer << 0;
			writer << 0;
			writer << (ipp_uchar_t)(n >> 8);
			writer << (ipp_uchar_t)n;
			writer.addOctets((const UInt8*) attr->name, (size_t)n);

			if (attr->value_tag > 0xff)
			{
				writer << IPP_TAG_EXTENSION;
				writer << (ipp_uchar_t)(attr->value_tag >> 24);
				writer << (ipp_uchar_t)(attr->value_tag >> 16);
				writer << (ipp_uchar_t)(attr->value_tag >> 8);
				writer << (ipp_uchar_t)attr->value_tag;
			}
			else
				writer << (ipp_uchar_t)attr->value_tag;

			writer << 0;
			writer << 0;
		}

		/*
		 * Now write the attribute value(s)...
		 */

		switch (attr->value_tag & ~IPP_TAG_CUPS_CONST)
		{
			case IPP_TAG_UNSUPPORTED_VALUE :
			case IPP_TAG_DEFAULT :
			case IPP_TAG_UNKNOWN :
			case IPP_TAG_NOVALUE :
			case IPP_TAG_NOTSETTABLE :
			case IPP_TAG_DELETEATTR :
			case IPP_TAG_ADMINDEFINE :
				writer << 0;
				writer << 0;
				break;

			case IPP_TAG_INTEGER :
			case IPP_TAG_ENUM :
				writeIntegersOrEnums(writer, attr);
				break;

			case IPP_TAG_BOOLEAN :
				writeBooleans(writer, attr);
				break;

			case IPP_TAG_TEXT :
			case IPP_TAG_NAME :
			case IPP_TAG_RESERVED_STRING :
			case IPP_TAG_KEYWORD :
			case IPP_TAG_URI :
			case IPP_TAG_URISCHEME :
			case IPP_TAG_CHARSET :
			case IPP_TAG_LANGUAGE :
			case IPP_TAG_MIMETYPE :
				writeStrings(writer, attr);
				break;

			case IPP_TAG_DATE :
				writeDates(writer, attr);
				break;

			case IPP_TAG_RESOLUTION :
				writeResolutions(writer, attr);
				break;

			case IPP_TAG_RANGE :
				writeRanges(writer, attr);
				break;

			case IPP_TAG_TEXTLANG :
			case IPP_TAG_NAMELANG :
				writeTextLangs(writer, attr);
				break;

			case IPP_TAG_BEGIN_COLLECTION :
				writeBeginCollection(writer, attr, ipp);
				break;

			default :
				writeDefaultUnknown(writer, attr);
				break;
		}
	}
}

ipp_state_t                /* O - Current state */
ippWriteIO2(void           *dst,        /* I - Destination */
			ipp_iocb_t cb,        /* I - Write callback function */
			ipp_t          *parent,        /* I - Parent IPP message */
			ipp_t          *ipp)        /* I - IPP data */
{
	ipp_state_t result = IPP_STATE_DATA;

	DEBUG_printf(("ippWriteIO(dst=%p, cb=%p, parent=%p, ipp=%p)", dst, cb, parent, ipp));

	if (dst == NULL || ipp == NULL)
		result = IPP_STATE_ERROR;
	else {
		IPPIOWriter writer(cb, dst);

		@try {
			ippWriteWithWriter(writer, parent, ipp);
		} @catch (NSException* e) {
			NSLog(@"Caught exception during write: %@", e);
			result = IPP_STATE_ERROR;
		}
	}

	return result;
}

static void ippWriteWithWriter(IPPIOWriter& writer, ipp_t* parent, ipp_t* ipp)
{
	switch (ipp->state)
	{
		case IPP_STATE_IDLE :
			ipp->state = (ipp_state_t) (1 + ipp->state); /* Avoid common problem... */
			// fall through

		case IPP_STATE_HEADER :
			writeHeader(writer, ipp, parent);
			// fall through

		case IPP_STATE_ATTRIBUTE :
			writeAllCurrentAttributes(writer, ipp, parent);

			if (ipp->current == NULL)
			{
				/*
				 * Done with all of the attributes; add the end-of-attributes
				 * tag or end-collection attribute...
				 */

				if (parent == NULL)
				{
					writer << IPP_TAG_END;
				}
				else
				{
					writer << IPP_TAG_END_COLLECTION;
					writer << 0; /* empty name */
					writer << 0;
					writer << 0; /* empty value */
					writer << 0;
				}

				ipp->state = IPP_STATE_DATA;
			}
			break;

		case IPP_STATE_DATA :
			break;

		default :
			break; /* anti-compiler-warning-code */
	}
}

