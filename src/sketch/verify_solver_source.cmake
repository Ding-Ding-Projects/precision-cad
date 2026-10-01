find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PRECISION_SOLVESPACE_SOURCE}" rev-parse HEAD
  RESULT_VARIABLE precision_git_result OUTPUT_VARIABLE precision_solvespace_head OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT precision_git_result EQUAL 0 OR NOT precision_solvespace_head STREQUAL PRECISION_SOLVESPACE_COMMIT)
  message(FATAL_ERROR "SolveSpace source commit is not the approved v3.2 commit.")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PRECISION_SOLVESPACE_SOURCE}" status --porcelain --untracked-files=all --ignore-submodules=none
  RESULT_VARIABLE precision_git_result OUTPUT_VARIABLE precision_solvespace_status OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT precision_git_result EQUAL 0 OR NOT precision_solvespace_status STREQUAL "")
  message(FATAL_ERROR "SolveSpace source or submodules have uncommitted changes; refusing unverified solver bytes.")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${PRECISION_SOLVESPACE_SOURCE}" submodule status --recursive extlib/eigen extlib/mimalloc
  RESULT_VARIABLE precision_git_result OUTPUT_VARIABLE precision_solvespace_submodules)
if(NOT precision_git_result EQUAL 0 OR precision_solvespace_submodules MATCHES "(^|\n)[+U-]")
  message(FATAL_ERROR "SolveSpace solver submodules are missing, conflicted, or differ from their pinned commits.")
endif()
