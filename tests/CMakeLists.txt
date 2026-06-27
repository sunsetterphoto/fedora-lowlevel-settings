find_package(Qt6 REQUIRED COMPONENTS Test)
enable_testing()

add_executable(test_backupmanager common/test_backupmanager.cpp)
target_link_libraries(test_backupmanager
    fls-common
    Qt6::Test
)
add_test(NAME test_backupmanager COMMAND test_backupmanager)

add_executable(test_configparser common/test_configparser.cpp)
target_link_libraries(test_configparser
    fls-common
    Qt6::Test
)
add_test(NAME test_configparser COMMAND test_configparser)

add_executable(test_helperpolicy helper/test_helperpolicy.cpp)
target_link_libraries(test_helperpolicy
    fls-helperpolicy
    Qt6::Test
)
add_test(NAME test_helperpolicy COMMAND test_helperpolicy)
