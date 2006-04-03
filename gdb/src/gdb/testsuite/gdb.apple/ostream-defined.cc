#include <fstream>

int main (int argc, const char * argv[]) {
    
    std::ofstream outStream ( "test.txt" );
    outStream << "Flasm" << std::endl; /* good place to put a breakpoint */
    return 0;
}
