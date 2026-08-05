find_package(PkgConfig)

if(NOT LIBUSB_FOUND)
  pkg_check_modules(LIBUSB_PKG QUIET libusb)

  find_path(LIBUSB_INCLUDE_DIR NAMES usb.h
    HINTS
    ${LIBUSB_PKG_INCLUDE_DIRS}
    PATHS
    /usr/include
    /usr/local/include
  )

  find_library(LIBUSB_LIBRARIES
    NAMES usb libusb
    HINTS
    ${LIBUSB_PKG_LIBRARY_DIRS}
    PATHS
    /usr/lib
    /usr/local/lib
  )

  if(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)
    set(LIBUSB_FOUND TRUE CACHE INTERNAL "legacy libusb found")
    message(STATUS "Found legacy libusb: ${LIBUSB_INCLUDE_DIR}, ${LIBUSB_LIBRARIES}")
  else()
    set(LIBUSB_FOUND FALSE CACHE INTERNAL "legacy libusb found")
    message(STATUS "legacy libusb not found.")
  endif()

  mark_as_advanced(LIBUSB_INCLUDE_DIR LIBUSB_LIBRARIES)
endif()
