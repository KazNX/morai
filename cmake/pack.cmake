set(CPACK_PACKAGE_VENDOR "KazNX")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY 
    "Morai fibre/microthreading library using C++ couroutines.")
set(CPACK_PACKAGE_DESCRIPTION
"Morai is a C++ fibre or microthread library using C++ coroutines. Fibres are a
lightweight cooperative multi-tasking mechanism running in a single thread."
)
set(CPACK_PACKAGE_CONTACT "KazNX")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/KazNX/morai")
set(CPACK_PACKAGE_VERSION "${MORAI_VERSION_MAJOR}.${MORAI_VERSION_MINOR}.${MORAI_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_MAJOR "${MORAI_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${MORAI_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${MORAI_VERSION_PATCH}")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "morai")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# set(CPACK_PACKAGE_ICON)
# set(CPACK_PACKAGE_EXECUTABLES)

if(LINUX)
  set(CPACK_GENERATOR "DEB")
endif(LINUX)

if(WIN32)
  set(CPACK_GENERATOR "WIX")
  # Add a custom target to convert the LICENSE to RTF for WIX packaging.
  # Read the license file
  file(READ "${CMAKE_SOURCE_DIR}/LICENSE" LICENSE_TEXT)
  # Escape backslashes, braces, and convert newlines to RTF line breaks
  string(REPLACE "\\" "\\\\" LICENSE_TEXT "${LICENSE_TEXT}")
  string(REPLACE "{" "\\{" LICENSE_TEXT "${LICENSE_TEXT}")
  string(REPLACE "}" "\\}" LICENSE_TEXT "${LICENSE_TEXT}")
  string(REPLACE "\n" "\\par " LICENSE_TEXT "${LICENSE_TEXT}")
  # Compose minimal RTF content
  set(RTF_CONTENT "{\\rtf1\\ansi\\deff0 {\\fonttbl {\\f0 Arial;}}\\f0\\fs20 ${LICENSE_TEXT}}")
  # Write to LICENSE.rtf
  file(WRITE "${CMAKE_BINARY_DIR}/LICENSE.rtf" "${RTF_CONTENT}")
endif()

# ---- Debian packing
set(CPACK_DEBIAN_PACKAGE_NAME "morai")
set(CPACK_DEB_COMPONENT_INSTALL ON)

set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}")
set(CPACK_DEBIAN_DEV_PACKAGE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}-dev")
# set(CPACK_DEBIAN_DOC_PACKAGE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}-doc")

set(CPACK_DEBIAN_RUNTIME_FILE_NAME "${CPACK_DEBIAN_RUNTIME_PACKAGE_NAME}.deb")
set(CPACK_DEBIAN_DEV_FILE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}.deb")
# set(CPACK_DEBIAN_DOC_FILE_NAME "${CPACK_DEBIAN_DOC_PACKAGE_NAME}.deb")

set(CPACK_DEBIAN_PACKAGE_EPOCH 0)

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "KazNX")

set(CPACK_DEBIAN_PACKAGE_CORE_DESCRIPTION "Morai C++ fibre library.")
set(CPACK_DEBIAN_PACKAGE_DOC_DESCRIPTION "Morai C++ fibre documentation.")

set(CPACK_DEBIAN_COMPRESSION_TYPE zstd)

# Only include Runtime and Dev components in the package to avoid installing FetchContent targets.
set(CPACK_COMPONENTS_ALL "Runtime;Dev")

# ---- Windows WIX packaging
set(CPACK_WIX_VERSION 4)
set(CPACK_WIX_UPGRADE_GUID "194ee3a5-cf89-4b4e-a248-a80de2219191")
set(CPACK_WIX_PRODUCT_GUID "3512de6e-d634-43e4-a8e9-f366eadfda34")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "Morai")
# set(CPACK_WIX_PRODUCT_ICON)

set(CPACK_WIX_LICENSE_RTF "${CMAKE_BINARY_DIR}/LICENSE.rtf")

include(CPack)
