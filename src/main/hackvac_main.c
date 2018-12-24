/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#ifndef GTEST_MAIN
#include "cpp_entry.h"
void app_main() {
  cpp_entry();
}
#else
#include "cpp_test_entry.h"
void app_main() {
  cpp_test_entry();
}
#endif
