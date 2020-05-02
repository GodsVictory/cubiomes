#include "generator.h"
#include "util.h"
#include <emscripten.h>

unsigned char rgb[4*512*512];
int *biomeCache;
LayerStack g;
unsigned char biomeColours[256][3];
unsigned int areaWidth = 256, areaHeight = 256;


unsigned char* EMSCRIPTEN_KEEPALIVE get_buffer() {
  initBiomes();
	g = setupGenerator(MC_1_15);
  // Allocate a sufficient buffer for the biomes and for the image pixels.
  biomeCache = allocCache(&g.layers[L_SHORE_16], areaWidth, areaHeight);
  // Initialize a colour map for biomes.
  initBiomeColours(biomeColours);
  return rgb;
}

void EMSCRIPTEN_KEEPALIVE gen_image(uint32_t lo, uint32_t hi) {
  int64_t s = (uint64_t)lo | ((uint64_t)hi << 32);

  // Extract the desired layer.
  Layer *layer = &g.layers[L_SHORE_16];

  int areaX = -128, areaZ = -128;
  unsigned int scale = 2;
  unsigned int imgWidth = areaWidth * scale, imgHeight = areaHeight * scale;

  // Apply the seed only for the required layers and generate the area.
  setWorldSeed(layer, s);
  genArea(layer, biomeCache, areaX, areaZ, areaWidth, areaHeight);

  // Map the biomes to a color buffer and save to an image.
  biomesToImage(rgb, biomeColours, biomeCache, areaWidth, areaHeight, scale, 2, 1);
}
