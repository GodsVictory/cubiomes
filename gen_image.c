#include "generator.h"
#include "util.h"
#include <emscripten.h>

unsigned char* EMSCRIPTEN_KEEPALIVE gen_image(uint32_t lo, uint32_t hi) {
  int64_t s = (uint64_t)lo | ((uint64_t)hi << 32);
  unsigned char biomeColours[256][3];
  initBiomes();

  // Initialize a colour map for biomes.
  initBiomeColours(biomeColours);

  // Extract the desired layer.
	LayerStack g = setupGenerator(MC_1_15);
  Layer *layer = &g.layers[L_SHORE_16];

  int areaX = -128, areaZ = -128;
  unsigned int areaWidth = 256, areaHeight = 256;
  unsigned int scale = 2;
  unsigned int imgWidth = areaWidth * scale, imgHeight = areaHeight * scale;

  // Allocate a sufficient buffer for the biomes and for the image pixels.
  int *biomes = allocCache(layer, areaWidth, areaHeight);
  unsigned char *rgb = (unsigned char *)malloc(4 * imgWidth * imgHeight);

  // Apply the seed only for the required layers and generate the area.
  setWorldSeed(layer, s);
  genArea(layer, biomes, areaX, areaZ, areaWidth, areaHeight);

  // Map the biomes to a color buffer and save to an image.
  biomesToImage(rgb, biomeColours, biomes, areaWidth, areaHeight, scale, 2, 1);

  // Clean up.
  free(biomes);
  freeGenerator(g);

  return rgb;
}
