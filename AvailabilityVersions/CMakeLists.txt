cmake_minimum_required(VERSION 3.19.6)

project("AvailabilityVersions" NONE)
add_library(${PROJECT_NAME} INTERFACE)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

set(APPLE_INTERNAL          "${DSTROOT}/AppleInternal")
set(OS_INCLUDE              "${DSTROOT}${SYSTEM_PREFIX}/usr/include/os")
set(USR_LOCAL_LIBEXEC       "${DSTROOT}${SYSTEM_PREFIX}/usr/local/libexec")
set(USR_LOCAL_INCLUDE       "${DSTROOT}${SYSTEM_PREFIX}/usr/local/include")
set(USR_INCLUDE             "${DSTROOT}${SYSTEM_PREFIX}/usr/include")
set(RUNTIME                 "${DSTROOT}${SYSTEM_PREFIX}/Runtime/usr/include")
set(RUNTIME_LOCAL           "${DSTROOT}${SYSTEM_PREFIX}/Runtime/usr/local/include")
set(DYLD_HEADERS_LOCATION   "${USR_LOCAL_INCLUDE}/dyld")
set(INSTALL_KERNEL_HEADERS  "${INSTALL_KERNEL_HEADERS}")

include(CMakePrintHelpers)
cmake_print_variables(SRCROOT)
cmake_print_variables(DSTROOT)
cmake_print_variables(OBJROOT)
cmake_print_variables(SYMROOT)
cmake_print_variables(APPLE_INTERNAL)
cmake_print_variables(CMAKE_CURRENT_BINARY_DIR)
cmake_print_variables(USR_LOCAL_LIBEXEC)
cmake_print_variables(USR_LOCAL_INCLUDE)
cmake_print_variables(USR_INCLUDE)
cmake_print_variables(RUNTIME)
cmake_print_variables(RUNTIME_LOCAL)
cmake_print_variables(CONFIG_EXCLAVEKIT)
cmake_print_variables(CONFIG_EXCLAVECORE)
cmake_print_variables(CONFIG_KERNELKIT)
cmake_print_variables(DRIVERKIT)

list(APPEND TEMPLATE_SOURCES  Availability.h AvailabilityInternal.h AvailabilityInternalLegacy.h AvailabilityInternalPrivate.h AvailabilityMacros.h
                              AvailabilityProhibitedInternal.h AvailabilityVersions.h VersionMap.h for_dyld_priv.inc os_availability.h )
list(APPEND UNIFDEF_SOURCES   os_availability.modulemap os_availability_private.modulemap )

list(APPEND LOCAL_MODULEMAPS  os_availability_private.modulemap)
list(APPEND PUBLIC_MODULEMAPS os_availability.modulemap)
list(APPEND LOCAL_HEADERS     AvailabilityInternalPrivate.h AvailabilityProhibitedInternal.h)
list(APPEND PUBLIC_HEADERS    Availability.h AvailabilityInternal.h AvailabilityInternalLegacy.h AvailabilityMacros.h AvailabilityVersions.h)
list(APPEND DYLD_HEADERS      VersionMap.h for_dyld_priv.inc)

macro(install_symlink filepath sympath)
  get_filename_component(parentpath ${sympath} DIRECTORY)
  make_directory(${parentpath})
  install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filepath} ${sympath})")
  install(CODE "message(\"-- Created symlink: ${sympath} -> ${filepath}\")")
endmacro(install_symlink)

add_custom_target(
    phony_target  ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/__non-existent-file"
)

add_custom_command(
    OUTPUT "${OBJROOT}/dsl.d"
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/__non-existent-file"
)

add_custom_command(
   OUTPUT "${OBJROOT}/availability"
   COMMAND "${SRCROOT}/availability" --av_version ${AV_VERSION} --preprocess ${SRCROOT}/availability ${OBJROOT}/availability
   DEPENDS "${SRCROOT}/availability"
   DEPENDS "${SRCROOT}/availability.dsl"
)

install(DIRECTORY DESTINATION ${APPLE_INTERNAL})

if (DRIVERKIT EQUAL "1")
  set(UNIFDEF_FLAGS -DBUILD_FOR_DRIVERKIT)
else()
  set(UNIFDEF_FLAGS -UBUILD_FOR_DRIVERKIT)
endif()

if (DRIVERKIT EQUAL "1")
  set(DYLD_HEADERS_LOCATION "${RUNTIME_LOCAL}/dyld")
  foreach(header ${PUBLIC_HEADERS})
    install(FILES "${OBJROOT}/${header}" DESTINATION ${RUNTIME} PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
  endforeach()

  list(REMOVE_ITEM PUBLIC_HEADERS AvailabilityMacros.h)
  list(REMOVE_ITEM LOCAL_HEADERS  AvailabilityProhibitedInternal.h)

  foreach(header ${PUBLIC_HEADERS})
    install(FILES "${OBJROOT}/${header}" DESTINATION ${RUNTIME} PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
  endforeach()

  install(FILES "${OBJROOT}/os_availability.h" DESTINATION "${RUNTIME}/os" RENAME availability.h PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

  foreach(header ${LOCAL_HEADERS})
    install(FILES "${OBJROOT}/${header}" DESTINATION ${RUNTIME_LOCAL} PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
  endforeach()
endif()

if(INSTALL_KERNEL_HEADERS EQUAL "1")
  foreach(header ${LOCAL_HEADERS})
    install_symlink("../../../../../../../usr/local/include/${header}" "${DSTROOT}/System/Library/Frameworks/Kernel.framework/Versions/A/PrivateHeaders/${header}")
  endforeach()

  install_symlink("../../../../../../../../usr/include/os/availability.h" "${DSTROOT}/System/Library/Frameworks/Kernel.framework/Versions/A/Headers/os/availability.h")

  foreach(header ${PUBLIC_HEADERS})
    install_symlink("../../../../../../../usr/include/${header}" "${DSTROOT}/System/Library/Frameworks/Kernel.framework/Versions/A/Headers/${header}")
  endforeach()
endif()

foreach(template_source ${TEMPLATE_SOURCES})
  add_custom_target("Generate-${template_source}" ALL DEPENDS "${OBJROOT}/${template_source}")
  add_custom_command(
    OUTPUT "${OBJROOT}/${template_source}"
    COMMAND ${OBJROOT}/availability --preprocess ${SRCROOT}/templates/${template_source}  ${OBJROOT}/${template_source}
    DEPENDS ${OBJROOT}/availability ${SRCROOT}/templates/${template_source}
  )
endforeach()

foreach(unifdef_source ${UNIFDEF_SOURCES})
  add_custom_target("Generate-${unifdef_source}" ALL DEPENDS "${OBJROOT}/${unifdef_source}")
  add_custom_command(
    OUTPUT "${OBJROOT}/${unifdef_source}"
    COMMAND sh "${SRCROOT}/unifdef_command.sh" ${UNIFDEF_FLAGS} -o "${OBJROOT}/${unifdef_source}" "${SRCROOT}/templates/${unifdef_source}"
    DEPENDS "${SRCROOT}/unifdef_command.sh" "${SRCROOT}/templates/${unifdef_source}"
  )
endforeach()

if(NOT CONFIG_KERNELKIT EQUAL "1")
  foreach(header ${DYLD_HEADERS})
    install(FILES "${OBJROOT}/${header}" DESTINATION ${DYLD_HEADERS_LOCATION} PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
  endforeach()
endif()

foreach(header ${LOCAL_HEADERS})
  install(FILES "${OBJROOT}/${header}" DESTINATION ${USR_LOCAL_INCLUDE} PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach()

foreach(modulemap ${LOCAL_MODULEMAPS})
  install(FILES "${OBJROOT}/${modulemap}" DESTINATION ${USR_LOCAL_INCLUDE} PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach()

install(FILES "${OBJROOT}/os_availability.h" DESTINATION ${OS_INCLUDE} RENAME availability.h PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

foreach(header ${PUBLIC_HEADERS})
  install(FILES "${OBJROOT}/${header}" DESTINATION ${USR_INCLUDE} PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach()

foreach(modulemap ${PUBLIC_MODULEMAPS})
  install(FILES "${OBJROOT}/${modulemap}" DESTINATION ${USR_INCLUDE} PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endforeach()


install(FILES "${OBJROOT}/availability"                   DESTINATION ${USR_LOCAL_LIBEXEC} RENAME availability.pl PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
