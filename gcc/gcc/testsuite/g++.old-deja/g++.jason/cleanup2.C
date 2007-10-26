// { dg-do assemble  }
// PRMS Id: 6303
// Bug: compiler crashes processing the cleanup for arrayOfClass.

// APPLE LOCAL mainline 2006-10-13 3904173
class Klasse {
public:
  // APPLE LOCAL mainline 2006-10-13 3904173
  ~Klasse();		// This dtor MUST be declared to generate the error...
};

// APPLE LOCAL mainline 2006-10-13 3904173
Klasse varOfClass;

int main() {
  // This MUST be 'const' to generate the error...
  // APPLE LOCAL mainline 2006-10-13 3904173
  const Klasse	arrayOfClass[1] = { varOfClass }; // causes abort
}
