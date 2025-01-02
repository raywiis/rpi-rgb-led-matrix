// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Small example how to use the library.
// For more examples, look at demo-main.cc
//
// This code is public domain
// (but note, that the led-matrix library this depends on is GPL v2)

#include "led-matrix.h"
#include "httplib.h"

#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <signal.h>

using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}


static void DrawOnCanvas(Canvas *canvas) {
  /*
   * Let's create a simple animation. We use the canvas to draw
   * pixels. We wait between each step to have a slower animation.
   */
  canvas->Fill(0, 0, 255);

  int center_x = canvas->width() / 2;
  int center_y = canvas->height() / 2;
  float radius_max = canvas->width() / 2;
  float angle_step = 1.0 / 360;
  for (float a = 0, r = 0; r < radius_max; a += angle_step, r += angle_step) {
    if (interrupt_received)
      return;
    float dot_x = cos(a * 2 * M_PI) * r;
    float dot_y = sin(a * 2 * M_PI) * r;
    canvas->SetPixel(center_x + dot_x, center_y + dot_y, 255, 0, 0);
    usleep(1 * 1000); // wait a little to slow down things.
  }
}

int main(int argc, char *argv[]) {
  RGBMatrix::Options defaults;
  defaults.hardware_mapping = "regular";  // or e.g. "adafruit-hat"
  defaults.rows = 32;
  defaults.chain_length = 1;
  defaults.parallel = 1;
  defaults.show_refresh_rate = true;
  defaults.cols = 64;

  Canvas *canvas = RGBMatrix::CreateFromFlags(&argc, &argv, &defaults);
  if (canvas == NULL)
    return 1;

  // It is always good to set up a signal handler to cleanly exit when we
  // receive a CTRL-C for instance. The DrawOnCanvas() routine is looking
  // for that.
  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  DrawOnCanvas(canvas);    // Using the canvas.

  using namespace httplib;

  Server svr;

  svr.Get("/hi", [](const Request& req, Response& res) {
    res.set_content("Hello World!", "text/plain");
  });

  svr.Post("/draw", [canvas](const Request& req, Response& res) {
    canvas->SetPixel(0, 0, 255, 255, 255);
    std::size_t bodySize = res.body.size();
    int canvasSize = canvas->height() * canvas->width();
    if (bodySize != (canvasSize * 3)) {
      res.set_content("size not ok", "text/plain");
      return;
    }
    canvas->Clear();
    for (int i = 0; i < canvasSize; i++) {
      int pos = i * 3;
      auto& r = res.body.at(pos);
      auto& g = res.body.at(pos + 1);
      auto& b = res.body.at(pos + 2);

      int x = i / canvas->width();
      int y = i % canvas->width();
      canvas->SetPixel(x, y, r, g, b);
    }
    res.set_content("ok", "text/plain");
  });

  svr.Get("/stop", [&](const Request& req, Response& res) {
    svr.stop();
  });

  int port = 1233;
  svr.listen("localhost", port);

  std::cout << "Listening on port: " << port << std::endl;


  // Animation finished. Shut down the RGB matrix.
  canvas->Clear();
  delete canvas;

  return 0;
}
