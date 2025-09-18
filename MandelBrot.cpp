#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

/* Mandelbrot Set Visualization */

/*
We have a complex number 'c' and going for the max iteration
we are computing if it escapes the boundary of value 2
as we know if it escapes beyond 2 then it is unstable.
*/

/*
    Efficiency Increasing:
    - The Mandelbrot set is symmetric about the real axis.
      We can compute only the upper half and mirror it to the lower half.

    - Many Points are known to be in the Mandelbrot set.
      We can skip computations for these points.
        - Points within the main cardioid -> Single check!
        - Points within the period-2 bulb -> Single check!

    -




*/

/* constants for the graphics!*/
const int WIDTH = 900;
const int HEIGHT = 900;
const int MAX_ITER = 100;
const int TILE_SIZE = 64; // Tile-based rendering for better load balancing

// Global variables for interaction
static Vector2 lastMousePos = {0, 0};
static bool isDragging = false;
static bool needsRedraw = true;
static const double MIN_MOVEMENT =
    2.0; // Minimum pixel movement to trigger redraw
static const double DRAG_SENSITIVITY =
    1.0; // Adjust this to make dragging faster/slower
static bool isFullscreen = false;

// Splash screen state
static bool showSplashScreen = true;
static double splashTime = 0.0;

/* Faster than std::complex!*/
struct Complex {
  double real;
  double imag;
};

/* Optimized Mandelbrot function with fast math */
// inline is hint to the compiler for optimization
inline int mandelbrotEscape(double cx, double cy, int max_iter) {
  // Quick escape checks first

  /* Cardioid check

    q = (x - 0.25)^2 + y^2
    (q * (q + (x - 0.25)) < 0.25 * y^2) -> inside cardioid

  */

  double q = (cx - 0.25) * (cx - 0.25) + cy * cy;
  if (q * (q + (cx - 0.25)) < 0.25 * cy * cy)
    return max_iter;

  /*
    Bulb Check
    (x + 1)^2 + y^2 < 1/16 -> inside period-2 bulb
  */

  if ((cx + 1) * (cx + 1) + cy * cy < 0.0625)
    return max_iter;

  /*
    Outside check
    x^2 + y^2 > 4 -> definitely outside

  */

  if (cx * cx + cy * cy > 4.0)
    return 0;

  // Fast iteration using registers
  double zx = 0, zy = 0;
  double zx2, zy2;
  int n = 0;

  /*
    The escape time algorithm is based on the iterative function:
        z(n+1) = z(n)² + c
    where z and c are complex numbers.

    In terms of real and imaginary parts:
        z = zx + i*zy
        c = cx + i*cy

    Squaring a complex number:
        z² = (zx + i*zy)² = zx² - zy² + 2*i*zx*zy

    Therefore, the iteration becomes:
        zx(n+1) = zx(n)² - zy(n)² + cx
        zy(n+1) = 2*zx(n)*zy(n) + cy
  */

  //  We unroll the loop for performance!
  do {
    zx2 = zx * zx;
    zy2 = zy * zy;
    zy = 2 * zx * zy + cy;
    zx = zx2 - zy2 + cx;
    n++;
  } while (zx2 + zy2 <= 4.0 && n < max_iter);

  return n;
}

/*
    Simple:
        We want to map left to right for the real part
        and top to bottom for the imaginary part

        So we have:
            real = real_min + (x / width) * (real_max - real_min)
            imag = imag_max - (y / height) * (imag_max - imag_min)

        this is because the y axis is inverted
        where the top left corner is at (0,0) and the
        bottom left corner is (0, height)

        so, we see that the imag_max is at the top and
        the imag_min is at the bottom and the real_min is
        at the left and the real_max is at the right

*/

// Draw ASCII art splash screen
void DrawSplashScreen(int screenWidth, int screenHeight, double time) {
  ClearBackground(BLACK);

  // Simple blinking
  float blink = sin(time * 3.0f);
  if (blink > 0) {
    const char *prompt = "Press ENTER or click to begin...";
    // Center the text by screenWidth - textWidth / 2 and screenHeight / 2
    DrawText(prompt, (screenWidth - MeasureText(prompt, 40)) / 2, screenHeight / 2, 40,
             YELLOW);
  }
}

void RenderTile(int tileX, int tileY, int width, int height, double Re_min,
                double Re_max, double Im_min, double Im_max,
                Color *pixelBuffer) {
  // Start of the tile in x direction
  int startX = tileX * TILE_SIZE;

  // End of the tile in x direction
  // Min because the tile might go out of bounds
  int endX = std::min(startX + TILE_SIZE, width);

  // Start of the tile in y direction
  int startY = tileY * TILE_SIZE;

  // End of the tile in y direction
  // Min because the tile might go out of bounds
  int endY = std::min(startY + TILE_SIZE, height);

  for (int y = startY; y < endY; y++) {
    for (int x = startX; x < endX; x++) {
      double real = Re_min + (x / (double)width) * (Re_max - Re_min);
      double imag = Im_max - (y / (double)height) * (Im_max - Im_min);
      int n = mandelbrotEscape(real, imag, MAX_ITER);

      // Map the number of iterations to a color
      Color color;
      if (n == MAX_ITER) {
        color = BLACK;
      } else {
        int hue = (int)(255.0 * n / MAX_ITER);
        color = ColorFromHSV(hue, 0.5f, 1.2f);
      }

      pixelBuffer[y * width + x] = color;
    }
  }
}

int main() {

  SetConfigFlags(FLAG_WINDOW_RESIZABLE); // Enable VSync and make window resizable
  InitWindow(WIDTH, HEIGHT, "Mandelbrot Explorer");

  SetTargetFPS(60); // Back to 60 FPS but with better control

  double Re_min = -2.0;
  double Re_max = 1.5;

  /* Imaginary value to consider from min to max*/
  double Im_min = -1.5;
  double Im_max = 1.5;

  // Image object but on the CPU Memory
  Image image = GenImageColor(WIDTH, HEIGHT, RAYWHITE);
  // Upload to the image from CPU to GPU and use VRAM for fast rendering
  Texture2D texture = LoadTextureFromImage(image);
  // Using Pixel Buffer to store the colors or cache
  // 1. Makes it easy to implement symmetric property of Mandelbrot
  // 2. Allows multi-threaded computation
  std::vector<Color> pixelBuffer(WIDTH * HEIGHT);

  // Force initial render
  bool hasRenderedOnce = false;

  // To Prevent overlapping renders cause of multi-threading
  bool isCurrentlyRendering = false;

  // Keep track of the current window size
  int currentWidth = WIDTH;
  int currentHeight = HEIGHT;

  while (!WindowShouldClose()) {
    splashTime += GetFrameTime();

    // Handle splash screen
    if (showSplashScreen) {
      BeginDrawing();
      DrawSplashScreen(currentWidth, currentHeight, splashTime);
      EndDrawing();

      // Check for input to dismiss splash screen
      if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
          IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        showSplashScreen = false;
        needsRedraw = true; // Force initial render
      }
      continue; // Skip the rest of the main loop
    }

    // Check if window size changed, to allow for dynamic resizing
    int newWidth = GetScreenWidth();
    int newHeight = GetScreenHeight();

    // If size changed, adjust complex plane bounds to maintain aspect ratio
    if (newWidth != currentWidth || newHeight != currentHeight) {
      // Calculate aspect ratio scaling
      double widthScale = (double)newWidth / currentWidth;
      double heightScale = (double)newHeight / currentHeight;

      // Scale the complex plane bounds to maintain proper aspect ratio
      double currentRealRange = Re_max - Re_min;
      double currentImagRange = Im_max - Im_min;
      double centerReal = (Re_min + Re_max) / 2.0;
      double centerImag = (Im_min + Im_max) / 2.0;

      // Scale the ranges
      double newRealRange = currentRealRange * widthScale;
      double newImagRange = currentImagRange * heightScale;

      // Update bounds around the center
      Re_min = centerReal - newRealRange / 2.0;
      Re_max = centerReal + newRealRange / 2.0;
      Im_min = centerImag - newImagRange / 2.0;
      Im_max = centerImag + newImagRange / 2.0;

      currentWidth = newWidth;
      currentHeight = newHeight;

      // Recreate image and texture with new size
      UnloadTexture(texture);
      UnloadImage(image);
      image = GenImageColor(currentWidth, currentHeight, RAYWHITE);
      texture = LoadTextureFromImage(image);
      pixelBuffer.resize(currentWidth * currentHeight);

      needsRedraw = true;
      hasRenderedOnce = false; // Force re-render with new size
    }

    // Handle mouse dragging for fractal panning
    Vector2 mousePos = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      isDragging = true;
      lastMousePos = mousePos;
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      isDragging = false;
    }

    // Handle fractal panning
    if (isDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      Vector2 mouseDelta = {mousePos.x - lastMousePos.x,
                            mousePos.y - lastMousePos.y};

      // Only process if movement is significant enough
      double deltaLength =
          sqrt(mouseDelta.x * mouseDelta.x + mouseDelta.y * mouseDelta.y);
      if (deltaLength >= MIN_MOVEMENT) {
        // Convert pixel movement to complex plane movement with sensitivity
        double realDelta =
            -mouseDelta.x * (Re_max - Re_min) / currentWidth * DRAG_SENSITIVITY;
        double imagDelta =
            mouseDelta.y * (Im_max - Im_min) / currentHeight * DRAG_SENSITIVITY;

        Re_min += realDelta;
        Re_max += realDelta;
        Im_min += imagDelta;
        Im_max += imagDelta;

        lastMousePos = mousePos;
        needsRedraw = true;
      }
    }

    // Handle zoom with mouse wheel
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
      double mouseRe =
          Re_min + (mousePos.x / (double)currentWidth) * (Re_max - Re_min);
      double mouseIm =
          Im_max - (mousePos.y / (double)currentHeight) * (Im_max - Im_min);

      double zoomFactor = (wheel > 0) ? 0.8 : 1.25; // Smoother zoom
      double newWidth = (Re_max - Re_min) * zoomFactor;
      double newHeight = (Im_max - Im_min) * zoomFactor;

      Re_min = mouseRe - newWidth / 2.0;
      Re_max = mouseRe + newWidth / 2.0;
      Im_min = mouseIm - newHeight / 2.0;
      Im_max = mouseIm + newHeight / 2.0;

      needsRedraw = true;
    }

    /* Keyboard controls */

    // Reset view with R key
    if (IsKeyPressed(KEY_R)) {
      Re_min = -2.0;
      Re_max = 1.5;
      Im_min = -1.5;
      Im_max = 1.5;
      needsRedraw = true;
    }

    // Toggle fullscreen with F key
    if (IsKeyPressed(KEY_F)) {
      if (isFullscreen) {
        SetWindowSize(WIDTH, HEIGHT);
        isFullscreen = false;
      } else {
        // Enter fullscreen
        isFullscreen = true;
      }
      needsRedraw = true; // Redraw when changing screen mode
    }

    // Minimize with M key
    if (IsKeyPressed(KEY_M)) {
      MinimizeWindow();
    }

    // Quit with Q key
    if (IsKeyPressed(KEY_Q)) {
      break;
    }

    /* Begin Drawing */
    // Only render when needed
    BeginDrawing();

    // Only recalculate if view changed or first time AND not currently
    // rendering
    if ((needsRedraw || !hasRenderedOnce) && !isCurrentlyRendering) {
      isCurrentlyRendering = true;
      ClearBackground(BLACK);

      // Create tiles for better load balancing
      int tilesX = (currentWidth + TILE_SIZE - 1) / TILE_SIZE;
      int tilesY = (currentHeight + TILE_SIZE - 1) / TILE_SIZE;
      int totalTiles = tilesX * tilesY;

      // Multi-threaded rendering
      std::vector<std::thread> threads;
      int numThreads = std::thread::hardware_concurrency();

      // Distribute tiles among threads
      for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([=, &pixelBuffer]() {
          for (int tileIdx = t; tileIdx < totalTiles; tileIdx += numThreads) {
            int tileX = tileIdx % tilesX;
            int tileY = tileIdx / tilesX;
            RenderTile(tileX, tileY, currentWidth, currentHeight, Re_min,
                       Re_max, Im_min, Im_max, pixelBuffer.data());
          }
        });
      }

      // Wait for all threads to finish
      for (auto &thread : threads) {
        thread.join();
      }

      // Update texture with new pixel data (GPU acceleration)
      UpdateTexture(texture, pixelBuffer.data());
      needsRedraw = false;
      hasRenderedOnce = true;
      isCurrentlyRendering = false;
    }

    // Only draw the texture if we have rendered at least once
    if (hasRenderedOnce) {
      DrawTexture(texture, 0, 0, WHITE);
    }

    // Show controls in bottom-left corner
    DrawText("Controls: F=Fullscreen, M=Minimize, R=Reset, Q=Quit", 10,
             currentHeight - 25, 16, LIME);

    // Add logo/watermark in top-right corner
    DrawText("MANDELBROT", currentWidth - 150, 10, 20, GOLD);
    DrawText("EXPLORER", currentWidth - 90, 35, 14, ORANGE);

    EndDrawing();
  }

  // Clean up resources
  UnloadTexture(texture);
  UnloadImage(image);

  CloseWindow();

  return 0;
}
