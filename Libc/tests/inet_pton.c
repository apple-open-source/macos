#include <darwintest.h>
#include <arpa/inet.h>

T_DECL(inet_pton_strip_leading_zeros, "Tests to verify inet_pton behavior") {

    // This address is the unambigously decimal
    const char* string_address_decimal = "10.20.30.40";
    const int int_address_decimal = 0x281e140a;

    // This address is not the same in octal and decimal
    const char* string_address_octal = "010.020.030.040";
    const int address_octal = 0x20181008;

    in_addr_t address_pton = 0;
    in_addr_t address_octal_pton = 0;

    inet_pton(AF_INET, string_address_decimal, &address_pton);
    inet_pton(AF_INET, string_address_octal, &address_octal_pton);

    // Ensure that inet_pton has the correct address for the decimal IP address
    T_ASSERT_TRUE(address_pton == int_address_decimal, "Address that is unambiguously decimal interpreted the same by pton");

    // Ensure that the leading zero address also is not interpreted as octal
    T_ASSERT_TRUE(address_octal_pton != address_octal, "Address with leading zeros was not interpreted as octal by pton");

    // Double check that the address is the same as the version without leading zero
    T_ASSERT_TRUE(address_octal_pton == int_address_decimal, "Address is interpreted the same as the address without leading zeros");
}
