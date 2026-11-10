# Shared by the product and the independent NON-RT Windows installer check.
file(SHA256 "${PROJECT_SOURCE_DIR}/tools/transcription/worker.py" mrs_python_worker_sha)
file(READ "${PROJECT_SOURCE_DIR}/tools/transcription/requirements-cpu.txt" mrs_python_requirements_text)
string(REPLACE "\r\n" "\n" mrs_python_requirements_text "${mrs_python_requirements_text}")
string(SHA256 mrs_python_requirements_sha "${mrs_python_requirements_text}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/transcription_profile.h" "#pragma once\nnamespace mrs { inline constexpr const char* transcription_worker_sha256 = \"${mrs_python_worker_sha}\"; inline constexpr const char* transcription_requirements_sha256 = \"${mrs_python_requirements_sha}\"; }\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tools/transcription/worker.py" "${PROJECT_SOURCE_DIR}/tools/transcription/requirements-cpu.txt")
