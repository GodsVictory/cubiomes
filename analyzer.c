#include "finders.h"
#include "generator.h"
#include "math.h"

long viable_count = 0;
long passed_filter = 0;
float step = 8;

void exiter(int64_t s)
{
    printf("%" PRId64, s);
    fflush(stdout);
    exit(1);
}

int main(int argc, char *argv[])
{
    initBiomes();

    int64_t s;
    unsigned int range, fullrange;
    BiomeFilter filter;
    int minscale;

    if (argc <= 1 || sscanf(argv[1], "%" PRId64, &s) != 1)
    {
        printf("Seed start: ");
        if (!scanf("%" SCNd64, &s))
        {
            printf("That's not right");
            exit(1);
        }
    }
    if (argc <= 2 || sscanf(argv[2], "%u", &range) != 1)
    {
        printf("Filter radius: ");
        if (!scanf("%i", &range))
        {
            printf("That's not right");
            exit(1);
        }
    }
    if (argc <= 3 || sscanf(argv[3], "%u", &fullrange) != 1)
    {
        printf("Full radius: ");
        if (!scanf("%i", &fullrange))
        {
            printf("That's not right");
            exit(1);
        }
    }

    enum BiomeID biomes[] = {ice_spikes, bamboo_jungle, desert, plains, ocean, jungle, forest, mushroom_fields, mesa, flower_forest, warm_ocean, frozen_ocean, megaTaiga, roofedForest, extremeHills, swamp, savanna, icePlains};
    filter = setupBiomeFilter(biomes,
                              sizeof(biomes) / sizeof(enum BiomeID));
    minscale = 256; // terminate search at this layer scale
                    // TODO: simple structure filter
    int ax = -range, az = -range;
    int w = 2 * range, h = 2 * range;
    int fw = 2 * fullrange, fh = 2 * fullrange;

    LayerStack* g = setupGenerator(MC_1_15);
    int *cache = allocCache(&g->layers[L_VORONOI_ZOOM_1], w, h);

    if (!checkForBiomes(g, cache, s, ax, az, w, h, filter, minscale))
        exiter(s);

    passed_filter++;
    applySeed(g, s);
    int x, z;

    //Pos goodhuts[2];
    int hut_count = 0;
    Pos huts[100];
    int huts_found = 0;
    int r = fullrange / SWAMP_HUT_CONFIG.regionSize;
    for (z = -r; z < r; z++)
    {
        for (x = -r; x < r; x++)
        {
            Pos p;
            p = getStructurePos(SWAMP_HUT_CONFIG, s, x, z);
            if (abs(p.x) < fullrange && abs(p.z) < fullrange)
            {
                if (isViableFeaturePos(Swamp_Hut, g, cache, p.x, p.z))
                {
                    huts[hut_count] = p;
                    hut_count++;
                    //printf("%i\n", huts[0].x);
                    for (int i = 0; i < hut_count; i++)
                    {
                        for (int j = 0; j < hut_count; j++)
                        {
                            if (j == i)
                                continue;
                            float dx, dz;
                            dx = abs(huts[i].x - huts[j].x);
                            dz = abs(huts[i].z - huts[j].z);
                            if (sqrt((dx * dx) + (dz * dz)) <= 200)
                            {
                                //goodhuts[0] = huts[i];
                                //goodhuts[1] = huts[j];
                                huts_found = 1;
                            }
                        }
                    }
                }
            }
        }
    }
    if (!huts_found)
        exiter(s);

    int monument_count = 0;
    r = fullrange / MONUMENT_CONFIG.regionSize;
    for (z = -r; z < r; z++)
    {
        for (x = -r; x < r; x++)
        {
            Pos p;
            p = getLargeStructurePos(MONUMENT_CONFIG, s, x, z);
            if (abs(p.x) < fullrange && abs(p.z) < fullrange)
                if (isViableOceanMonumentPos(g, cache, p.x, p.z))
                    monument_count++;
        }
    }
    if (monument_count == 0)
        exiter(s);

    int ocean_count = 0;
    // biome enum defined in layers.h
    enum BiomeID req_biomes[] = {ice_spikes, bamboo_jungle, desert, plains, ocean, jungle, forest, mushroom_fields, mesa, flower_forest, warm_ocean, frozen_ocean, megaTaiga, roofedForest, extremeHills, swamp, savanna, icePlains};
    int biome_exists[sizeof(req_biomes) / sizeof(enum BiomeID)] = {0};
    enum BiomeID biome_percent[] = {badlands, badlands_plateau, bamboo_jungle, bamboo_jungle_hills, basalt_deltas, beach, birch_forest, birch_forest_hills, cold_ocean, crimson_forest, dark_forest, dark_forest_hills, deep_cold_ocean, deep_frozen_ocean, deep_lukewarm_ocean, deep_ocean, deep_warm_ocean, desert, desert_hills, desert_lakes, end_barrens, end_highlands, end_midlands, eroded_badlands, flower_forest, forest, frozen_ocean, frozen_river, giant_spruce_taiga, giant_spruce_taiga_hills, giant_tree_taiga, giant_tree_taiga_hills, gravelly_mountains, ice_spikes, jungle, jungle_edge, jungle_hills, lukewarm_ocean, modified_badlands_plateau, modified_gravelly_mountains, modified_jungle, modified_jungle_edge, modified_wooded_badlands_plateau, mountain_edge, mountains, mushroom_fields, mushroom_field_shore, nether_wastes, ocean, plains, river, savanna, savanna_plateau, shattered_savanna, shattered_savanna_plateau, small_end_islands, snowy_beach, snowy_mountains, snowy_taiga, snowy_taiga_hills, snowy_taiga_mountains, snowy_tundra, soul_sand_valley, stone_shore, sunflower_plains, swamp, swamp_hills, taiga, taiga_hills, taiga_mountains, tall_birch_forest, tall_birch_hills, the_end, the_void, warm_ocean, warped_forest, wooded_badlands_plateau, wooded_hills, wooded_mountains};
    char *biome_percent_string[sizeof(biome_percent) / sizeof(enum BiomeID)] = {"badlands", "badlands_plateau", "bamboo_jungle", "bamboo_jungle_hills", "basalt_deltas", "beach", "birch_forest", "birch_forest_hills", "cold_ocean", "crimson_forest", "dark_forest", "dark_forest_hills", "deep_cold_ocean", "deep_frozen_ocean", "deep_lukewarm_ocean", "deep_ocean", "deep_warm_ocean", "desert", "desert_hills", "desert_lakes", "end_barrens", "end_highlands", "end_midlands", "eroded_badlands", "flower_forest", "forest", "frozen_ocean", "frozen_river", "giant_spruce_taiga", "giant_spruce_taiga_hills", "giant_tree_taiga", "giant_tree_taiga_hills", "gravelly_mountains", "ice_spikes", "jungle", "jungle_edge", "jungle_hills", "lukewarm_ocean", "modified_badlands_plateau", "modified_gravelly_mountains", "modified_jungle", "modified_jungle_edge", "modified_wooded_badlands_plateau", "mountain_edge", "mountains", "mushroom_fields", "mushroom_field_shore", "nether_wastes", "ocean", "plains", "river", "savanna", "savanna_plateau", "shattered_savanna", "shattered_savanna_plateau", "small_end_islands", "snowy_beach", "snowy_mountains", "snowy_taiga", "snowy_taiga_hills", "snowy_taiga_mountains", "snowy_tundra", "soul_sand_valley", "stone_shore", "sunflower_plains", "swamp", "swamp_hills", "taiga", "taiga_hills", "taiga_mountains", "tall_birch_forest", "tall_birch_hills", "the_end", "the_void", "warm_ocean", "warped_forest", "wooded_badlands_plateau", "wooded_hills", "wooded_mountains"};
    int biome_percent_counter[sizeof(biome_percent) / sizeof(enum BiomeID)] = {0};

    r = fullrange;
    for (z = -r; z < r; z += step)
    {
        for (x = -r; x < r; x += step)
        {
            Pos p = {x, z};
            int biome = getBiomeAtPos(g, p);
            if (isOceanic(biome))
                ocean_count++;
            if (abs(x) < range && abs(z) < range)
                for (int i = 0; i < sizeof(biome_exists) / sizeof(int); i++)
                    if (biome == req_biomes[i])
                        biome_exists[i] = -1;
            for (int i = 0; i < sizeof(biome_percent) / sizeof(enum BiomeID); i++)
                if (biome == biome_percent[i])
                    biome_percent_counter[i]++;
        }
    }

    //verify all biomes are present
    int all_biomes = 1;
    for (int i = 0; i < sizeof(req_biomes) / sizeof(enum BiomeID); i++)
        if (biome_exists[i] != -1)
            all_biomes = 0;
    if (!all_biomes)
        exiter(s);

    //get spawn biome
    Pos spawn = getSpawn(MC_1_15, g, cache, s);
    int spawn_biome = getBiomeAtPos(g, spawn);
    char *spawn_biome_string;
    for (int i = 0; i < sizeof(biome_percent) / sizeof(enum BiomeID); i++)
        if (spawn_biome == biome_percent[i])
            spawn_biome_string = biome_percent_string[i];

    //get closest village
    Pos villages[500] = {0};
    int village_count = 0;
    r = fullrange / VILLAGE_CONFIG.regionSize;
    for (z = -r; z < r; z++)
    {
        for (x = -r; x < r; x++)
        {
            Pos p;
            p = getStructurePos(VILLAGE_CONFIG, s, x, z);
            if (abs(p.x) < fullrange && abs(p.z) < fullrange)
                if (isViableVillagePos(g, cache, p.x, p.z))
                    villages[village_count++] = p;
        }
    }

    //Pos closest_village;
    int closest_village_distance = -1;
    for (int i = 0; i < sizeof(villages) / sizeof(villages[0]); i++)
    {
        if (villages[i].x == 0 && villages[i].z == 0)
            break;
        float dx, dz;
        dx = abs(spawn.x - villages[i].x);
        dz = abs(spawn.z - villages[i].z);
        if (sqrt((dx * dx) + (dz * dz)) < closest_village_distance || closest_village_distance == -1)
        {
            //closest_village = villages[i];
            closest_village_distance = sqrt((dx * dx) + (dz * dz));
        }
    }

    char out[512];
    snprintf(out, 512, "%" PRId64, s);
    snprintf(out + strlen(out), 512 - strlen(out), ",%s", spawn_biome_string);
    snprintf(out + strlen(out), 512 - strlen(out), ",%i", closest_village_distance);
    snprintf(out + strlen(out), 512 - strlen(out), ",%i", hut_count);
    snprintf(out + strlen(out), 512 - strlen(out), ",%i", monument_count);
    for (int i = 0; i < sizeof(biome_percent_counter) / sizeof(int); i++)
        snprintf(out + strlen(out), 512 - strlen(out), ",%.2f", (biome_percent_counter[i] * (step * step) / (fw * fh)) * 100);
    //snprintf(out + strlen(out), 512 - strlen(out), "\n");
    printf("%s", out);
    fflush(stdout);

    return 0;
}
