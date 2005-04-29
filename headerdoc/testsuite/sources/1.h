/****************************
*
* Copyright (c) 2002, 2003, Bob Frank
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
*  - Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*  - Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
*  - Neither the name of Log4Cocoa nor the names of its contributors or owners
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
****************************/

#import <Foundation/Foundation.h>
#import "L4Layout.h"

/*!
	@defined L4InvalidSpecifierException
	@abstract Defines the name for invalid format specifier exceptions
	@updated 2004-03-14
*/
#define L4InvalidSpecifierException 		@"L4InvalidSpecifierException"

/*!
	@defined L4NoConversionPatternException
	@abstract Defines the name for the no conversion pattern exception
	@discussion This exception is thrown if you try to use an L4PatternLayout before setting its conversion pattern
	@updated 2004-03-14
*/
#define L4NoConversionPatternException 		@"L4NoConversionPatternException"

/*!
	@defined L4InvalidBraceClauseException
	@abstract Defines the name for the invalid brace clause exception
	@discussion Some of the format specifiers can be followed by content surrounded by braces ({}).  When this brace clause is invalid for any reason, the L4InvalidBraceClauseException is thrown
	@updated 2004-03-14
*/
#define L4InvalidBraceClauseException		@"L4InvalidBraceClauseException"

/*!
	@defined L4PatternLayoutDefaultSpecifiers
	@abstract An NSCharacterSet that contains the default format specifier characters
	@updated 2004-03-14
*/
#define L4PatternLayoutDefaultSpecifiers	[NSCharacterSet characterSetWithCharactersInString: @"CdFlLmMnpr%"]

/*!
	@defined L4PatternLayoutTrailingBracesSpecifiers
	@abstract An NSCharacterSet that contains the subset of characters from L4PatternLayoutDefaultSpecifiers that can have a brace clause after the character
	@updated 2004-03-14
*/
#define L4PatternLayoutTrailingBracesSpecifiers	[NSCharacterSet characterSetWithCharactersInString: @"Cd"]

@class L4LoggingEvent;
@class L4PatternParser;

/*!
	@class L4PatternLayout
	@abstract A layout that uses a conversion pattern to format logging messages
	@discussion <P>
 A flexible layout configurable with pattern string.

 <p>The goal of this class is to <CODE>format</CODE> a <CODE>LoggingEvent</CODE> and return the results as a String. The results
 depend on the <em>conversion pattern</em>.

 <p>The conversion pattern is closely related to the conversion
 pattern of the printf function in C. A conversion pattern is
 composed of literal text and format control expressions called
 <em>format specifiers</em>.

 <p><i>You are free to insert any literal text within the conversion
 pattern.</i>

 <p>Each conversion specifier starts with a percent sign (%) and is
 followed by optional <em>format modifiers</em> and a <em>specifier
 character</em>. The specifier character specifies the type of
 data, e.g. priority, date. The format
 modifiers control such things as field width, padding, left and
 right justification. The following is a simple example.

 <p>Let the conversion pattern be <b>"%-5p : %m%n"</b> and assume
 that the Log4Cocoa environment was set to use a L4PatternLayout. Then the
 statements
 <pre>
 log4Debug("Message 1");
 log4Warn("Message 2");
 </pre>
 would yield the output
 <pre>
 DEBUG : Message 1
 WARN  : Message 2
 </pre>

 <p>Note that there is no explicit separator between text and
 conversion specifiers. The pattern parser knows when it has reached
 the end of a conversion specifier when it reads a specifier
 character. In the example, above the conversion specifier
 <b>%-5p</b> means the priority of the logging event should be left
 justified to a width of five characters.

 The recognized conversion characters are

 <p>
 <table border="1" CELLPADDING="8">
 <th>Conversion Character</th>
 <th>Effect</th>

 <tr>
 <td align=center><b>C</b></td>

 <td>Used to output the fully qualified class name of the logger
 issuing the logging request. This conversion specifier
 can be optionally followed by <em>precision specifier</em>, that
 is a decimal constant in braces.

 <p>If a precision specifier is given, then only the corresponding
 number of right most components of the logger name will be
 printed. By default the logger name is output in fully qualified form.

 <p>For example, for the class name "NSObject.MyClass.SubClass.SomeClass", the
 pattern <b>%C{1}</b> will output "SomeClass".

 </td>
 </tr>

 <tr>
 <td align=center><b>d</b></td>

 <td>Used to output the date of the logging event. The date conversion specifier may be followed by a <em>date format specifier</em> enclosed between braces. For example, <b>%d{%H:%M:%S}</b>.  If no date format specifier is given, then  an international format of "%Y-%m-%d %H:%M:%S %z" is used.

 <p>The date format specifier admits the same syntax as the calendar format string in <CODE>NSCalendarDate</CODE>'s descriptionWithCalendarFormat: method.
 </td>
 </tr>

 <tr>
 <td align=center><b>F</b></td>

 <td>Used to output the file name where the logging request was
 issued.</td>
 </tr>

 <tr>
 <td align=center><b>l</b></td>

 <td>Used to output location information of the caller which generated
 the logging event.

 <p>The location information consists of the fully qualified name of the calling logger, the method the log request originated in, followed by the log request's source file name and line number between parentheses.

 </td>
 </tr>

 <tr>
 <td align=center><b>L</b></td>

 <td>Used to output the line number from where the logging request
 was issued.</td>

 </tr>


 <tr>
 <td align=center><b>m</b></td>
 <td>Used to output the application supplied message associated with
 the logging event.</td>
 </tr>

 <tr>
 <td align=center><b>M</b></td>

 <td>Used to output the method name where the logging request was
 issued.</td>

 </tr>

 <tr>
 <td align=center><b>n</b></td>

 <td>Outputs the line separator character, <code>\n</code>.</td>

 </tr>

 <tr>
 <td align=center><b>p</b></td>
 <td>Used to output the priority (level) of the logging event.</td>
 </tr>

 <tr>

 <td align=center><b>r</b></td>

 <td>Used to output the number of milliseconds elapsed since the start
 of the application until the creation of the logging event.</td>
 </tr>

 <tr>

 <td align=center><b>%</b></td>

 <td>The sequence %% outputs a single percent sign.
 </td>
 </tr>

 </table>

 <p>By default the relevant information is output as is. However,
 with the aid of format modifiers it is possible to change the
 minimum field width, the maximum field width and justification.

 <p>The optional format modifier is placed between the percent sign
 and the conversion character.

 <p>The first optional format modifier is the <em>left justification
 flag</em> which is just the minus (-) character. Then comes the
 optional <em>minimum field width</em> modifier. This is a decimal
 constant that represents the minimum number of characters to
 output. If the data item requires fewer characters, it is padded on
 either the left or the right until the minimum width is
 reached. The default is to pad on the left (right justify) but you
 can specify right padding with the left justification flag. The
 padding character is space. If the data item is larger than the
 minimum field width, the field is expanded to accommodate the
 data. The value is never truncated.

 <p>This behavior can be changed using the <em>maximum field
 width</em> modifier which is designated by a period followed by a
 decimal constant. If the data item is longer than the maximum
 field, then the extra characters are removed from the
 <em>beginning</em> of the data item and not from the end. For
 example, it the maximum field width is eight and the data item is
 ten characters long, then the first two characters of the data item
 are dropped. This behavior deviates from the printf function in C
 where truncation is done from the end.

 <p>Below are various format modifier examples for the category
 conversion specifier.

 <p>
 <TABLE BORDER=1 CELLPADDING=8>
 <th>Format modifier
 <th>left justify
 <th>minimum width
 <th>maximum width
 <th>comment

 <tr>
 <td align=center>%20C</td>
 <td align=center>false</td>
 <td align=center>20</td>
 <td align=center>none</td>

 <td>Left pad with spaces if the logger name is less than 20
 characters long.

 <tr> <td align=center>%-20C</td> <td align=center>true</td> <td
 align=center>20</td> <td align=center>none</td> <td>Right pad with
 spaces if the logger name is less than 20 characters long.

 <tr>
 <td align=center>%.30C</td>
 <td align=center>NA</td>
 <td align=center>none</td>
 <td align=center>30</td>

 <td>Truncate from the beginning if the logger name is longer than 30
 characters.

 <tr>
 <td align=center>%20.30C</td>
 <td align=center>false</td>
 <td align=center>20</td>
 <td align=center>30</td>

 <td>Left pad with spaces if the logger name is shorter than 20
 characters. However, if logger name is longer than 30 characters,
 then truncate from the beginning.

 <tr>
 <td align=center>%-20.30C</td>
 <td align=center>true</td>
 <td align=center>20</td>
 <td align=center>30</td>

 <td>Right pad with spaces if the logger name is shorter than 20
 characters. However, if logger name is longer than 30 characters,
 then truncate from the beginning.

 </table>
 
 @updated 2004-03-14
*/
@interface L4PatternLayout : L4Layout
{
	NSString*			_conversionPattern;
	id					_parserDelegate;
	id					_converterDelegate;

	@private
	NSMutableArray*		_tokenArray;
}

/*!
	@method init
	@abstract Initializes an L4PatternLayout with the default conversion pattern, %m%n
	@discussion Calls initWthConversionPattern: with the string "%m%n"
	@result A newly initialized L4PatternLayout object
*/
- (id)init;

/*!
	@method initWithConversionPattern:
	@abstract Initializes an L4PatternLayout with a custom conversion pattern
	@param cp The custom conversion pattern
	@result A newly initialized L4PatternLayout object
*/
- (id)initWithConversionPattern: (NSString*)cp;

/*!
	@method format:
	@abstract Uses this class's conversion pattern to format logging messages
	@param event A logging event that contains information that the layout needs to format the logging message
	@result	A formatted logging message that adheres to the L4PatternLayout's conversion pattern
*/
- (NSString *)format: (L4LoggingEvent *)event;

/*!
	@method conversionPattern
	@abstract Returns the pattern layout's conversion pattern
	@result The pattern layout's conversion pattern
*/
- (NSString*)conversionPattern;

/*!
	@method setConversionPattern:
	@abstract Sets the pattern layout's conversion pattern
	@param cp The new conversion pattern
*/
- (void)setConversionPattern: (NSString*)cp;

/*!
	@method parserDelegate
	@abstract Returns the pattern layout's parser delegate object
	@result	The pattern layout's parser delegate
*/
- (id)parserDelegate;

/*!
	@method setParserDelegate:
	@abstract Sets the optional parser delegate object for this pattern layout
	@discussion When the pattern layout formats logging messages, it first takes the conversion pattern and parses it into token strings.  You can provide a parser delegate to override how a pattern layout parses the conversion pattern into token strings.  By default, the pattern layout divides the conversion pattern into a series of literal strings and format specifiers.

		The parser delegate must respond to the parseConversionPattern:intoArray: method.
	@param pd The new parser delegate
*/
- (void)setParserDelegate: (id)pd;

/*!
	@method converterDelegate
	@abstract Returns the pattern layout's converter delegate object
	@result The pattern layout's converter delegate
*/
- (id)converterDelegate;

/*!
	@method setConverterDelegate:
	@abstract Sets the optional converter delegate object for this pattern layout
	@discussion When the pattern layout formats logging messages, it first takes the conversion pattern and parses it into token strings.  Then, it takes each token string and converts it into the corresponding part of the final formatted message.  You can provide a converter delegate to override or extend how a pattern layout converts its token strings.  By default, the pattern layout does nothing to literal string tokens and converts format specifiers as explained in the description of this class.

		The converter delegate must respond to the convertTokenString:withLoggingEvent:intoString: method.
	@param cd The new converter delegate
*/
- (void)setConverterDelegate: (id)cd;

@end

/*!
	@category NSObject(L4PatternLayoutConverterDelegate)
	@abstract Declares methods that an L4PatternLayout's converter delegate must implement.
	@updated 2004-03-14
 */
@interface NSObject (L4PatternLayoutConverterDelegate)

/*!
	@method convertTokenString:withLoggingEvent:intoString:
	@abstract Allows an object to override or extend how an L4PatternLayout converts its token strings into pieces of the final formatted logging message
	@param token The token string to convert
	@param logEvent An L4LoggingEvent that contains information about the current logging request
	@param convertedString A reference to an NSString that will contain the result of the token string's conversion upon exiting the method
	@result Return YES to indicate that you have converted the token string.  Return NO to let the pattern layout's default behavior attempt to convert it.
 */
- (BOOL)convertTokenString: (NSString*)token withLoggingEvent: (L4LoggingEvent*)logEvent intoString: (NSString**)convertedString;

@end

/*!
	@category NSObject(L4PatternLayoutParserDelegate)
	@abstract Declares methods that an L4PatternLayout's parser delegate must implement.
	@updated 2004-03-14
*/
@interface NSObject (L4PatternLayoutParserDelegate)

/*!
	@method parseConversionPattern:intoArray:
	@abstract Allows an object to override how an L4PatternLayout parses its conversion pattern into a series of token strings
	@param cp The conversion pattern to be parsed
	@param tokenStringArray A mutable array to hold the parsed tokens
*/
- (void)parseConversionPattern: (NSString*)cp intoArray: (NSMutableArray**)tokenStringArray;

@end
