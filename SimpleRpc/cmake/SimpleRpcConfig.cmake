include(CMakeFindDependencyMacro)

################################################################################
## Find dependencies ###########################################################
################################################################################

find_dependency(Homa)

################################################################################
## Add target file #############################################################
################################################################################
include("${CMAKE_CURRENT_LIST_DIR}/SimpleRpcTargets.cmake")