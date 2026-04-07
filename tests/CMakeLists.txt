find_package(Qt6 REQUIRED COMPONENTS Test)
enable_testing()

add_executable(test_backupmanager common/test_backupmanager.cpp)
target_link_libraries(test_backupmanager
    nsa-fsm-common
    Qt6::Test
)
add_test(NAME test_backupmanager COMMAND test_backupmanager)
