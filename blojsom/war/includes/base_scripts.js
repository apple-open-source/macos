function searchFieldFocus(theSearchField)
{
	placeholderString = theSearchField.form.placeholder.value;
	
	if (theSearchField.value == placeholderString)
	{
		theSearchField.value = '';
		theSearchField.style.color='black';
	}
}

function searchFieldBlur(theSearchField)
{
	if (theSearchField.value == '')
	{
		theSearchField.value = placeholderString;
		theSearchField.style.color='gray';
	}
}

function hideOrShowLoginDialog(whichOne)
{
	document.getElementById('login_dialog').style.visibility = ( whichOne ? 'visible' : 'hidden' );
}

function hideSystemMessageIfNecessary()
{
	if (document.getElementById('blojsom_system_message'))
	{
		document.getElementById('blojsom_system_message').style.visibility = 'hidden';
	}
}

function hideOrShowSettingsDialog(whichOne)
{
	hideSystemMessageIfNecessary();
	document.getElementById('settings_dialog').style.visibility = ( whichOne ? 'visible' : 'hidden' );
	
	if (whichOne)
	{
		document.getElementById('blog-name').focus();
		hideOrShowNewEntryDialog(false);
		hideOrShowNewCategoryDialog(false);
	}
}

function hideOrShowNewEntryDialog(whichOne)
{
	hideSystemMessageIfNecessary();
	document.getElementById('entry_dialog').style.visibility = ( whichOne ? 'visible' : 'hidden' );
	
	if (whichOne)
	{
		document.getElementById('adding_entry_title').focus();
		hideOrShowSettingsDialog(false);
		hideOrShowNewCategoryDialog(false);
	}
}

function clickedAdvancedSettingsButton(theButton)
{
	var theTable = document.getElementById('entry_dialog_table');
	
	// if the button says "Advanced"...
	if (theButton.value == theButton.form.new_entry_advanced_text.value)
	{
		// build the RSS enclosure row
		var theRSSEnclosureRow = document.createElement('tr');
		var theRSSEnclosureHeaderCell = document.createElement('th');
		var theRSSEnclosureHeaderText = document.createTextNode(theButton.form.rss_enclosure_label_text.value);
		var theRSSEnclosureFieldCell = document.createElement('td');
		var theRSSEnclosureField = document.createElement('input');
		theRSSEnclosureField.setAttribute('type', 'file');
		theRSSEnclosureField.setAttribute('name', 'upload_enclosure');
		theRSSEnclosureField.setAttribute('id', 'upload_enclosure');
		theRSSEnclosureField.setAttribute('size', '30');
		theRSSEnclosureFieldCell.appendChild(theRSSEnclosureField);
		theRSSEnclosureHeaderCell.appendChild(theRSSEnclosureHeaderText);
		theRSSEnclosureRow.appendChild(theRSSEnclosureHeaderCell);
		theRSSEnclosureRow.appendChild(theRSSEnclosureFieldCell);
	
		// build the trackback field row
		var theTrackbackRow = document.createElement('tr');
		var theTrackbackHeaderCell = document.createElement('th');
		var theTrackbackHeaderText = document.createTextNode(theButton.form.trackback_label_text.value);
		var theTrackbackFieldCell = document.createElement('td');
		var theTrackbackField = document.createElement('input');
		theTrackbackField.setAttribute('type', 'text');
		theTrackbackField.setAttribute('name', 'blog-trackback-urls');
		theTrackbackField.setAttribute('id', 'blog-trackback-urls');
		theTrackbackField.setAttribute('size', '50');
		theTrackbackFieldCell.appendChild(theTrackbackField);
		theTrackbackHeaderCell.appendChild(theTrackbackHeaderText);
		theTrackbackRow.appendChild(theTrackbackHeaderCell);
		theTrackbackRow.appendChild(theTrackbackFieldCell);
		
		// build the trackback explanation row
		var theTrackbackExpRow = document.createElement('tr');
		var theTrackbackExpText = document.createTextNode(theButton.form.trackback_description_text.value);
		var theTrackbackExpCell = document.createElement('td');
		theTrackbackExpCell.className = 'setting_label';
		theTrackbackExpCell.appendChild(theTrackbackExpText);
		theTrackbackExpRow.appendChild(document.createElement('td'));
		theTrackbackExpRow.appendChild(theTrackbackExpCell);
		
		// add the rows to the table
		var theButtonsRow = theTable.getElementsByTagName('tr').item(2);
		theButtonsRow.parentNode.insertBefore(theRSSEnclosureRow, theButtonsRow);
		theButtonsRow.parentNode.insertBefore(theTrackbackRow, theButtonsRow);
		theButtonsRow.parentNode.insertBefore(theTrackbackExpRow, theButtonsRow);
		
		// switch the Advanced button to 'Simple'
		theButton.value = theButton.form.new_entry_simple_text.value;
	}
	else if (theButton.value == theButton.form.new_entry_simple_text.value)
	{
		// remove the extra rows
		var theTrackbackRow = theTable.getElementsByTagName('tr').item(2);
		theTrackbackRow.parentNode.removeChild(theTrackbackRow);
		theTrackbackRow = theTable.getElementsByTagName('tr').item(2);
		theTrackbackRow.parentNode.removeChild(theTrackbackRow);
		theTrackbackRow = theTable.getElementsByTagName('tr').item(2);
		theTrackbackRow.parentNode.removeChild(theTrackbackRow);
	
		// switch the Simple button to 'Advanced'
		theButton.value = theButton.form.new_entry_advanced_text.value;
	}
}

function hideOrShowNewCategoryDialog(whichOne)
{
	hideSystemMessageIfNecessary();
	document.getElementById('category_dialog').style.visibility = (whichOne ? 'visible' : 'hidden' );
	
	if (whichOne)
	{
		document.getElementById('newcat_category_name').focus();
		hideOrShowSettingsDialog(false);
		hideOrShowNewEntryDialog(false);
	}
}

function tryFocusOnEditField()
{
	if (document.getElementById('editing_entry_title'))
	{
		document.getElementById('editing_entry_title').focus();
	}
}

function clickedCancelEditButton(theButton)
{
	document.getElementById('editing_action').value = '';
	theButton.form.submit();
}

function deleteCategory(confirmMessage)
{
	categoryName = document.getElementById('deleting_category_description').value;
	confirmMessage = confirmMessage.replace("%@", categoryName);
	
	if (confirm(confirmMessage))
	{
		document.getElementById('deleting_category_form').submit();
	}
}

function ridConfirmMessage()
{
	document.getElementById('blojsom_system_message').style.visibility = 'hidden';
}

function showConfirmMessage()
{
	if (document.getElementById('blojsom_system_message'))
	{
		var messageText = document.getElementById('blojsom_system_message').innerHTML;
		
		if (messageText.indexOf(" ") == 0)
		{
			messageText = messageText.substr(1, messageText.length - 1);
			alert(messageText);
		}
		else
		{
			document.getElementById('blojsom_system_message').style.visibility = 'visible';
			var currentTimer = setTimeout('ridConfirmMessage()', 8000);
		}
	}

	if (showLoginDialogOnLoad)
	{
		hideOrShowLoginDialog(true);
		document.getElementById('username').focus();
	}
}

function validateNewEntryForm(theForm)
{
	var emptyMessage = theForm.elements['field_empty_msg'].value;
	var entryTitle = theForm.elements['adding_entry_title'].value;
	var entryDescription = theForm.elements['adding_entry_desc'].value;
	
	hideOrShowNewEntryDialog(false);
	
	if ((entryTitle == '') || (entryDescription == ''))
	{
		alert(emptyMessage);
		return false;
	}
	
	return true;
}

function validateNewCategoryForm(theForm)
{
	var categoryPopup = theForm.elements['newcat_super'];
	var selectedOption = categoryPopup.options[categoryPopup.selectedIndex];

	hideOrShowNewCategoryDialog(false);
	
	theForm.elements['new_category_parent_label'].value = selectedOption.text;
	
	return true;
}

var showLoginDialogOnLoad = false;
