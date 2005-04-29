//  An example using the Metakit C++ persistence library

/////////////////////////////////////////////////////////////////////////////
//
//  This code demonstrates:
//
//    - Creating a persistent view and adding two data rows to it.
//    - Adding a third data row using Metakit's operator shorthands.
//    - Adding an additional property without losing the existing data.
//    - Storing an additional view in the data file later on.
//    - Inserting a new record into one of the views in the datafile.
//    - Real persistence, the data file will grow each time this is run.
//
/////////////////////////////////////////////////////////////////////////////

#include "mk4.h"
#include "mk4str.h"

#include <stdio.h>

int
#if _WIN32_WCE
_cdecl
#endif
main()
{
    // These properties could just as well have been declared globally.
  c4_StringProp pName ("name");
  c4_StringProp pCountry ("country");

    // Note: be careful with the lifetime of views vs. storage objects.
    // When a storage object goes away, all associated views are cleared.
  c4_Storage storage ("myfile.dat", true);

// There are two ways to make views persistent, but the c4_View::Store call
// call used in previous demos will be dropped, use "c4_View::GetAs" instead.
  
    // Start with an empty view of the proper structure.
  c4_View vAddress = storage.GetAs("address[name:S,country:S]");

    // Let's add two rows of data to the view.
  c4_Row row;

  pName (row) = "John Williams";
  pCountry (row) = "UK";
  vAddress.Add(row);

  pName (row) = "Paco Pena";
  pCountry (row) = "Spain";
  vAddress.Add(row);

    // A simple check to prove that the data is in the view.
  c4_String s1 (pName (vAddress[1]));
  c4_String s2 (pCountry (vAddress[1]));
  printf("The country of %s is: %s\n", (const char*) s1, (const char*) s2);

    // This saves the data to file.
  storage.Commit(); // Data file now contains 2 addresses.

    // A very compact notation to create and add a third row.
  vAddress.Add(pName ["Julien Coco"] + pCountry ["Netherlands"]);

  storage.Commit(); // Data file now contains 3 addresses.

    // Add a third property to the address view ("on-the-fly").
  vAddress = storage.GetAs("address[name:S,country:S,age:I]");

    // Set the new age property in one of the existing addresses.
  c4_IntProp pAge ("age");
  pAge (vAddress[1]) = 44;

  storage.Commit(); // Data file now contains 3 addresses with age field.

    // Add a second view to the data file, leaving the first view intact.
  c4_View vInfo = storage.GetAs("info[version:I]");

    // Add some data, a single integer in this case.
  c4_IntProp pVersion ("version");
  vInfo.Add(pVersion [100]);

  storage.Commit(); // Data file now contains 3 addresses and 1 info rec.

    // Insert a row into the address view.  Note that another (duplicate)
    // property definition is used here - just to show it can be done.
  c4_IntProp pYears ("age");  // On file this is still the "age" field.

  vAddress.InsertAt(2, pName ["Julian Bream"] + pYears [50]);

    // Preceding commits were only included for demonstration purposes.
  storage.Commit(); // Datafile now contains 4 addresses and 1 info rec.

    // To inspect the data file, use the dump utility: "DUMP MYFILE.DAT".
    // It should generate the following output:
    //
    //    myfile.dat: 3 properties
    //      address[name:S,country:S,age:I],info[version:I]
    //
    //     VIEW   1 rows = address:V info:V
    //      0: subview 'address'
    //       VIEW   4 rows = name:S country:S age:I
    //        0: 'John Williams' 'UK' 0
    //        1: 'Paco Pena' 'Spain' 44
    //        2: 'Julian Bream' '' 50
    //        3: 'Julien Coco' 'Netherlands' 0
    //      0: subview 'info'
    //       VIEW   1 rows = version:I
    //        0: 100
    //
    // Note: results will differ if this program is run more than once.

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
