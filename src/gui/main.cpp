#include "connection.h"
#include "log.h"
#include "main_window.h"

int main(int argc, char *argv[]) {
  LOG_INFO("Remote desk");
  MainWindow main_window;
  Connection connection;

  connection.DeskConnectionInit();
  connection.DeskConnectionCreate("123456");
  // connection.Create("123456", 800, 600);

  main_window.Run();

  return 0;
}