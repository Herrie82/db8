# Copyright (c) 2013-2018 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0



project(gtest_dbclient C CXX)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(${PROJECT_NAME}
               MojDbClientTest.cpp
               MojDbClientTestResultHandler.cpp
               MojDbClientTestWatcher.cpp
               Environment.cpp)


target_link_libraries(${PROJECT_NAME} gtest gtest_main)

target_link_libraries(${PROJECT_NAME}
                      mojocore
                      mojodb
                      mojoluna)



install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
        DESTINATION ${WEBOS_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}/tests
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ)
