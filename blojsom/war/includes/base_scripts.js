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
