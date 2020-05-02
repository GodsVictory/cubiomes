#include "finders.h"
#include "generator.h"
#include "math.h"
#include <time.h>
#include <signal.h>
#include "util.h"

struct compactinfo_t
{
	int64_t seedStart, seedEnd;
	unsigned int range;
	unsigned int fullrange;
	BiomeFilter filter;
	int withHut, withMonument;
	int minscale;
	int thread_id;
	char genimage;
};

int64_t count = 0;
int64_t c[128] = {0};
int64_t last_count = 0;
long viable_count = 0;
long last_viable_count = 0;
long passed_filter = 0;
float sps = 0;
char eta[20];
time_t start_time;
int64_t total_seeds = 0;
float max_ocean = 25; //maximum amount of ocean allowed in percentage
float step = 8;
float min_major_biomes = 0; //minimum major biome percent
int printing = 0;
int exited = 0;
int exit_counter = 0;
int raw = 0;

void intHandler()
{
	if (!exited)
	{
		fprintf(stderr, "\r%*c", 128, ' ');
		printf("\nStopping all threads...\n");
		fflush(stdout);
		exited = 1;
	}
}

#ifdef USE_PTHREAD
static void *statTracker()
#else
static DWORD WINAPI statTracker()
#endif
{
	time_t last_time = time(NULL);
	while (!exited)
	{
		time_t this_time = time(NULL);
		if (this_time - last_time < 1)
			continue;
		count = 0;
		for (int i = 0; i < 128; i++)
			count += c[i];
		sps = (count) / (this_time - start_time);
		if (sps > 0)
		{
			time_t predict_end = this_time + (double)total_seeds / sps;
			strftime(eta, 20, "%H:%M:%S", localtime(&predict_end));
		}
		float percent_done = (double)count / (double)total_seeds * 100;
		if (percent_done < 0)
			percent_done = 0;

		unsigned int elapsed = this_time - start_time;
		int elapsed_days = -1;
		int elapsed_hours = -1;
		int elapsed_minutes = -1;
		int elapsed_seconds = -1;
		if (elapsed < 8553600)
		{
			elapsed_days = elapsed / 60 / 60 / 24;
			elapsed_hours = elapsed / 60 / 60 % 24;
			elapsed_minutes = elapsed / 60 % 60;
			elapsed_seconds = elapsed % 60;
		}

		unsigned int eta = 0;
		int eta_days = -1;
		int eta_hours = -1;
		int eta_minutes = -1;
		int eta_seconds = -1;
		if (sps > 0)
		{
			eta = (double)(total_seeds - count) / sps;
			if (eta < 8553600)
			{
				eta_days = eta / 60 / 60 / 24;
				eta_hours = eta / 60 / 60 % 24;
				eta_minutes = eta / 60 % 60;
				eta_seconds = eta % 60;
			}
		}

		fprintf(stderr, "\rscanned: %18" PRId64 " | viable: %3li | sps: %9.0lf | elapsed: %02i:%02i:%02i:%02i", count, viable_count, sps, elapsed_days, elapsed_hours, elapsed_minutes, elapsed_seconds);
		if (eta > 0 || percent_done > 0)
			fprintf(stderr, " | %6.2lf%% | eta: %02i:%02i:%02i:%02i  ", percent_done, eta_days, eta_hours, eta_minutes, eta_seconds);
		fflush(stdout);
		last_time = this_time;
		last_count = count;
		last_viable_count = viable_count;
	}

#ifdef USE_PTHREAD
	pthread_exit(NULL);
#endif
	return 0;
}

#ifdef USE_PTHREAD
static void *searchCompactBiomesThread(void *data)
#else
static DWORD WINAPI searchCompactBiomesThread(LPVOID data)
#endif
{
	struct compactinfo_t info = *(struct compactinfo_t *)data;
	int ax = -info.range, az = -info.range;
	int w = 2 * info.range, h = 2 * info.range;
	int fw = 2 * info.fullrange, fh = 2 * info.fullrange;
	int64_t s;

	LayerStack g = setupGenerator(MC_1_15);
	int *cache = allocCache(&g.layers[L_VORONOI_ZOOM_1], w, h);

	for (s = info.seedStart; s != info.seedEnd; s++)
	{
		if (exited)
			break;

		c[info.thread_id]++;

		if (!checkForBiomes(&g, cache, s, ax, az, w, h, info.filter, info.minscale))
			continue;

		passed_filter++;
		applySeed(&g, s);
		int x, z;

		Pos goodhuts[2];
		int hut_count = 0;
		if (info.withHut)
		{
			Pos huts[100];
			int huts_found = 0;
			int r = info.fullrange / SWAMP_HUT_CONFIG.regionSize;
			for (z = -r; z < r; z++)
			{
				for (x = -r; x < r; x++)
				{
					Pos p;
					p = getStructurePos(SWAMP_HUT_CONFIG, s, x, z);
					if (isViableFeaturePos(Swamp_Hut, g, cache, p.x, p.z))
					{
						if (abs(p.x) < info.fullrange && abs(p.z) < info.fullrange)
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
										//printf("%i\n", counter);
										//printf("%f\n",(sqrt((dx*dx)+(dz*dz))));
										//printf("%i, %i, %i, %i\n",huts[i].x,huts[i].z,huts[j].x,huts[j].z);
										goodhuts[0] = huts[i];
										goodhuts[1] = huts[j];
										huts_found = 1;
									}
									if (exited)
										break;
									//if (huts_found)
									//	break;
								}
								if (exited)
									break;
								//if (huts_found)
								//	break;
							}
						}
					}
					if (exited)
						break;
					//if (huts_found)
					//	break;
				}
				if (exited)
					break;
				//if (huts_found)
				//	break;
			}
			if (exited)
				break;
			if (!huts_found)
				continue;
		}

		int monument_count = 0;
		if (info.withMonument)
		{
			int r = info.fullrange / MONUMENT_CONFIG.regionSize;
			for (z = -r; z < r; z++)
			{
				for (x = -r; x < r; x++)
				{
					Pos p;
					p = getLargeStructurePos(MONUMENT_CONFIG, s, x, z);
					if (isViableOceanMonumentPos(g, cache, p.x, p.z))
						if (abs(p.x) < info.fullrange && abs(p.z) < info.fullrange)
							monument_count++;

					if (exited)
						break;
					//if (monument_count > 0)
					//	break;
				}
				if (exited)
					break;
				//if (monument_count > 0)
				//	break;
			}
			if (exited)
				break;
			if (monument_count == 0)
				continue;
		}

		int ocean_count = 0;
		// biome enum defined in layers.h
		enum BiomeID req_biomes[] = {ice_spikes, bamboo_jungle, desert, plains, ocean, jungle, forest, mushroom_fields, mesa, flower_forest, warm_ocean, frozen_ocean, megaTaiga, roofedForest, extremeHills, swamp, savanna, icePlains};
		int biome_exists[sizeof(req_biomes) / sizeof(enum BiomeID)] = {0};
		enum BiomeID major_biome_percent[11][16] = {
			{desert, desert_lakes, desert_hills},
			{plains, sunflower_plains},
			{extremeHills, mountains, wooded_mountains, gravelly_mountains, modified_gravelly_mountains, mountain_edge},
			{jungle, jungle_hills, modified_jungle, jungle_edge, modified_jungle_edge, bamboo_jungle, bamboo_jungle_hills},
			{forest, wooded_hills, flower_forest, birch_forest, birch_forest_hills, tall_birch_forest, tall_birch_hills},
			{roofedForest, dark_forest, dark_forest_hills},
			{badlands, badlands_plateau, modified_badlands_plateau, wooded_badlands_plateau, modified_wooded_badlands_plateau, eroded_badlands},
			{swamp, swamp_hills},
			{savanna, savanna_plateau, shattered_savanna, shattered_savanna_plateau},
			{ice_spikes, snowy_beach, snowy_mountains, snowy_taiga, snowy_taiga_hills, snowy_taiga_mountains, snowy_tundra},
			{taiga, taiga_hills, taiga_mountains, snowy_taiga, snowy_taiga_hills, snowy_taiga_mountains, giant_tree_taiga, giant_tree_taiga_hills, giant_spruce_taiga, giant_spruce_taiga_hills}};
		char *major_biome_percent_string[11] = {"desert", "plains", "hills & mountains", "jungle", "forest", "roofed forest", "mesa", "swamp", "savanna", "ice & snow", "taiga"};
		int major_biome_percent_counter[11] = {0};
		enum BiomeID biome_percent[] = {badlands, badlands_plateau, bamboo_jungle, bamboo_jungle_hills, basalt_deltas, beach, birch_forest, birch_forest_hills, cold_ocean, crimson_forest, dark_forest, dark_forest_hills, deep_cold_ocean, deep_frozen_ocean, deep_lukewarm_ocean, deep_ocean, deep_warm_ocean, desert, desert_hills, desert_lakes, end_barrens, end_highlands, end_midlands, eroded_badlands, flower_forest, forest, frozen_ocean, frozen_river, giant_spruce_taiga, giant_spruce_taiga_hills, giant_tree_taiga, giant_tree_taiga_hills, gravelly_mountains, ice_spikes, jungle, jungle_edge, jungle_hills, lukewarm_ocean, modified_badlands_plateau, modified_gravelly_mountains, modified_jungle, modified_jungle_edge, modified_wooded_badlands_plateau, mountain_edge, mountains, mushroom_fields, mushroom_field_shore, nether_wastes, ocean, plains, river, savanna, savanna_plateau, shattered_savanna, shattered_savanna_plateau, small_end_islands, snowy_beach, snowy_mountains, snowy_taiga, snowy_taiga_hills, snowy_taiga_mountains, snowy_tundra, soul_sand_valley, stone_shore, sunflower_plains, swamp, swamp_hills, taiga, taiga_hills, taiga_mountains, tall_birch_forest, tall_birch_hills, the_end, the_void, warm_ocean, warped_forest, wooded_badlands_plateau, wooded_hills, wooded_mountains};
		char *biome_percent_string[sizeof(biome_percent) / sizeof(enum BiomeID)] = {"badlands", "badlands_plateau", "bamboo_jungle", "bamboo_jungle_hills", "basalt_deltas", "beach", "birch_forest", "birch_forest_hills", "cold_ocean", "crimson_forest", "dark_forest", "dark_forest_hills", "deep_cold_ocean", "deep_frozen_ocean", "deep_lukewarm_ocean", "deep_ocean", "deep_warm_ocean", "desert", "desert_hills", "desert_lakes", "end_barrens", "end_highlands", "end_midlands", "eroded_badlands", "flower_forest", "forest", "frozen_ocean", "frozen_river", "giant_spruce_taiga", "giant_spruce_taiga_hills", "giant_tree_taiga", "giant_tree_taiga_hills", "gravelly_mountains", "ice_spikes", "jungle", "jungle_edge", "jungle_hills", "lukewarm_ocean", "modified_badlands_plateau", "modified_gravelly_mountains", "modified_jungle", "modified_jungle_edge", "modified_wooded_badlands_plateau", "mountain_edge", "mountains", "mushroom_fields", "mushroom_field_shore", "nether_wastes", "ocean", "plains", "river", "savanna", "savanna_plateau", "shattered_savanna", "shattered_savanna_plateau", "small_end_islands", "snowy_beach", "snowy_mountains", "snowy_taiga", "snowy_taiga_hills", "snowy_taiga_mountains", "snowy_tundra", "soul_sand_valley", "stone_shore", "sunflower_plains", "swamp", "swamp_hills", "taiga", "taiga_hills", "taiga_mountains", "tall_birch_forest", "tall_birch_hills", "the_end", "the_void", "warm_ocean", "warped_forest", "wooded_badlands_plateau", "wooded_hills", "wooded_mountains"};
		int biome_percent_counter[sizeof(biome_percent) / sizeof(enum BiomeID)] = {0};

		int r = info.fullrange;
		for (z = -r; z < r; z += step)
		{
			for (x = -r; x < r; x += step)
			{
				Pos p = {x, z};
				int biome = getBiomeAtPos(g, p);
				if (isOceanic(biome))
					ocean_count++;
				if (abs(x) < info.range && abs(z) < info.range)
					for (int i = 0; i < sizeof(biome_exists) / sizeof(int); i++)
						if (biome == req_biomes[i])
							biome_exists[i] = -1;
				if (!isOceanic(biome))
					for (int i = 0; i < sizeof(major_biome_percent_counter) / sizeof(int); i++)
						for (int j = 0; j < sizeof(major_biome_percent[i]) / sizeof(enum BiomeID); j++)
							if (biome == major_biome_percent[i][j])
								major_biome_percent_counter[i]++;
				for (int i = 0; i < sizeof(biome_percent) / sizeof(enum BiomeID); i++)
					if (biome == biome_percent[i])
						biome_percent_counter[i]++;
				if (exited)
					break;
			}
			if (exited)
				break;
		}
		if (exited)
			break;

		//check for max ocean percent
		float ocean_percent = (ocean_count * (step * step) / (fw * fh)) * 100;
		//if (ocean_percent > max_ocean)
		//	continue;

		//check for minimum major biome percent
		int major_biome_less_than_min = 1;
		for (int i = 0; i < sizeof(major_biome_percent_counter) / sizeof(int); i++)
			if ((major_biome_percent_counter[i] * (step * step) / (fw * fh)) * 100 < min_major_biomes)
				major_biome_less_than_min = 0;
		if (!major_biome_less_than_min)
			continue;

		//verify all biomes are present
		int all_biomes = 1;
		for (int i = 0; i < sizeof(req_biomes) / sizeof(enum BiomeID); i++)
			if (biome_exists[i] != -1)
				all_biomes = 0;
		if (!all_biomes)
			continue;

		//get spawn biome
		Pos spawn = getSpawn(MC_1_15, &g, cache, s);
		int spawn_biome = getBiomeAtPos(g, spawn);
		char *spawn_biome_string;
		for (int i = 0; i < sizeof(biome_percent) / sizeof(enum BiomeID); i++)
			if (spawn_biome == biome_percent[i])
				spawn_biome_string = biome_percent_string[i];

		//get closest village
		Pos villages[500] = {0};
		int village_count = 0;
		r = info.fullrange / VILLAGE_CONFIG.regionSize;
		for (z = -r; z < r; z++)
		{
			for (x = -r; x < r; x++)
			{
				Pos p;
				p = getLargeStructurePos(VILLAGE_CONFIG, s, x, z);
				if (isViableVillagePos(g, cache, p.x, p.z))
					if (abs(p.x) < info.fullrange && abs(p.z) < info.fullrange)
						villages[village_count++] = p;
				if (exited)
					break;
			}
			if (exited)
				break;
		}
		if (exited)
			break;

		Pos closest_village;
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
				closest_village = villages[i];
				closest_village_distance = sqrt((dx * dx) + (dz * dz));
			}
		}

		viable_count++;

		char out[512];
		snprintf(out, 512, "%" PRId64, s);
		snprintf(out + strlen(out), 512 - strlen(out), ",%i", hut_count);
		snprintf(out + strlen(out), 512 - strlen(out), ",%i", monument_count);
		for (int i = 0; i < sizeof(biome_percent_counter) / sizeof(int); i++)
			snprintf(out + strlen(out), 512 - strlen(out), ",%.2f%%", (biome_percent_counter[i] * (step * step) / (fw * fh)) * 100);
		snprintf(out + strlen(out), 512 - strlen(out), "\n");

		if (raw)
		{
			printf("%s", out);
			fflush(stdout);
		}
		else
		{
			char info_out[512];
			snprintf(info_out, 512, "\n%17s: %" PRId64, "Found", s);
			snprintf(info_out + strlen(info_out), 512 - strlen(info_out), "\n%17s: %s", "Spawn biome", spawn_biome_string);
			snprintf(info_out + strlen(info_out), 512 - strlen(info_out), "\n%17s: %i,%i", "Closest village", closest_village.x, closest_village.z);
			snprintf(info_out + strlen(info_out), 512 - strlen(info_out), "\n%17s: %i,%i & %i,%i", "Huts", goodhuts[0].x, goodhuts[0].z, goodhuts[1].x, goodhuts[1].z);
			snprintf(info_out + strlen(info_out), 512 - strlen(info_out), "\n%17s: %5.2f%%", "Ocean", ocean_percent);
			for (int i = 0; i < sizeof(major_biome_percent_counter) / sizeof(int); i++)
				snprintf(info_out + strlen(info_out), 512 - strlen(info_out), "\n%17s: %5.2f%%", major_biome_percent_string[i], (major_biome_percent_counter[i] * (step * step) / (fw * fh)) * 100);
			snprintf(info_out + strlen(info_out), 512 - strlen(info_out), "\n");
			fprintf(stderr, "\r%*c", 128, ' ');
			fflush(stdout);
			printf("%s", info_out);
			fflush(stdout);
		}

		FILE *fp = fopen("found.csv", "r");
		if (fp == NULL)
		{
			fp = fopen("found.csv", "a");
			fprintf(fp, "seed");
			fprintf(fp, ",huts");
			fprintf(fp, ",monuments");
			for (int i = 0; i < sizeof(biome_percent_counter) / sizeof(int); i++)
				fprintf(fp, ",%s", biome_percent_string[i]);
			fprintf(fp, "\n");
		}
		else
		{
			fclose(fp);
			fp = fopen("found.csv", "a");
		}
		//fprintf(fp, "%" PRId64, s);
		//fprintf(fp, ",%i:%i & %i:%i", goodhuts[0].x, goodhuts[0].z, goodhuts[1].x, goodhuts[1].z);
		//fprintf(fp, ",%.2f%%", ocean_percent);

		//fprintf(fp, ",%i", hut_count);
		//fprintf(fp, ",%i", monument_count);

		//for (int i = 0; i < sizeof(biome_percent_counter) / sizeof(int); i++)
		//	fprintf(fp, ",%.2f%%", (biome_percent_counter[i] * (step * step) / (fw * fh)) * 100);

		//for (int i = 0; i < sizeof(major_biome_counter) / sizeof(int); i++)
		//	fprintf(fp, ",%.2f%%", (major_biome_counter[i] * (step * step) / (fw * fh)) * 100);
		fprintf(fp, "%s", out);
		fclose(fp);

		if (info.genimage == 'y')
		{
			unsigned char biomeColours[256][3];

			// Initialize a colour map for biomes.
			initBiomeColours(biomeColours);

			// Extract the desired layer.
			Layer *layer = &g.layers[L_SHORE_16];

			int areaX = -128, areaZ = -128;
			unsigned int areaWidth = 256, areaHeight = 256;
			unsigned int scale = 2;
			unsigned int imgWidth = areaWidth * scale, imgHeight = areaHeight * scale;

			// Allocate a sufficient buffer for the biomes and for the image pixels.
			int *biomes = allocCache(layer, areaWidth, areaHeight);
			unsigned char *rgb = (unsigned char *)malloc(3 * imgWidth * imgHeight);

			// Apply the seed only for the required layers and generate the area.
			setWorldSeed(layer, s);
			genArea(layer, biomes, areaX, areaZ, areaWidth, areaHeight);

			// Map the biomes to a color buffer and save to an image.
			biomesToImage(rgb, biomeColours, biomes, areaWidth, areaHeight, scale, 2, 0);
			char filename[20];
			snprintf(filename, 20, "%" PRId64 ".svg", s);
			saveSVG(filename, rgb, imgWidth, imgHeight);

			// Clean up.
			free(biomes);
			free(rgb);
		}
	}

	freeGenerator(g);
	free(cache);

#ifdef USE_PTHREAD
	pthread_exit(NULL);
#endif
	return 0;
}

int main(int argc, char *argv[])
{
	initBiomes();

	int64_t seedStart, seedEnd;
	unsigned int threads, t, range, fullrange;
	char genimage;
	BiomeFilter filter;
	int withHut, withMonument;
	int minscale;

	// arguments
	if (argc <= 0)
	{
		printf("find_compactbiomes [seed_start] [seed_end] [threads] [range]\n"
			   "\n"
			   "  seed_start    starting seed for search [long, default=0]\n"
			   "  end_start     end seed for search [long, default=-1]\n"
			   "  threads       number of threads to use [uint, default=1]\n"
			   "  range         search range (in blocks) [uint, default=1024]\n");
		exit(1);
	}
	if (argc <= 1 || sscanf(argv[1], "%" PRId64, &seedStart) != 1)
	{
		printf("Seed start: ");
		if (!scanf("%" SCNd64, &seedStart))
		{
			printf("That's not right");
			exit(1);
		}
	}
	if (argc <= 2 || sscanf(argv[2], "%" PRId64, &seedEnd) != 1)
	{
		printf("Seed end: ");
		if (!scanf("%" SCNd64, &seedEnd))
		{
			printf("That's not right");
			exit(1);
		}
	}
	if (argc <= 3 || sscanf(argv[3], "%u", &threads) != 1)
	{
		printf("Threads: ");
		if (!scanf("%i", &threads))
		{
			printf("That's not right");
			exit(1);
		}
	}
	if (argc <= 4 || sscanf(argv[4], "%u", &range) != 1)
	{
		printf("Filter radius: ");
		if (!scanf("%i", &range))
		{
			printf("That's not right");
			exit(1);
		}
	}
	if (argc <= 5 || sscanf(argv[5], "%u", &fullrange) != 1)
	{
		printf("Full radius: ");
		if (!scanf("%i", &fullrange))
		{
			printf("That's not right");
			exit(1);
		}
	}
	if (argc <= 6 || sscanf(argv[6], "%c", &genimage) != 1)
	{
		printf("Generate images? [y/n]: ");
		if (!scanf(" %c", &genimage))
		{
			printf("That's not right");
			exit(1);
		}
	}
	if (argc <= 7 || sscanf(argv[7], "%u", &raw) != 1)
	{
		raw = 0;
	}

	enum BiomeID biomes[] = {ice_spikes, bamboo_jungle, desert, plains, ocean, jungle, forest, mushroom_fields, mesa, flower_forest, warm_ocean, frozen_ocean, megaTaiga, roofedForest, extremeHills, swamp, savanna, icePlains};
	filter = setupBiomeFilter(biomes,
							  sizeof(biomes) / sizeof(enum BiomeID));
	minscale = 256; // terminate search at this layer scale
	// TODO: simple structure filter
	withHut = 1;
	withMonument = 1;
	start_time = time(NULL);

	if (!raw)
	{
		printf("Build: 41\n");
		printf("Starting search through seeds %" PRId64 " to %" PRId64 ", using %u threads.\n"
			   "Search radius = %u.\n",
			   seedStart, seedEnd, threads, range);
		char time_start[20];
		strftime(time_start, 20, "%m/%d/%Y %H:%M:%S", localtime(&start_time));
		printf("Started: %s\n", time_start);
	}

	thread_id_t threadID[threads];
	struct compactinfo_t info[threads];

	// store thread information
	if (seedStart == 0 && seedEnd == -1)
	{
		seedStart = -999999999999999999;
		seedEnd = 999999999999999999;
	}
	total_seeds = (uint64_t)seedEnd - (uint64_t)seedStart;
	uint64_t seedCnt = ((uint64_t)seedEnd - (uint64_t)seedStart) / threads;
	for (t = 0; t < threads; t++)
	{
		info[t].seedStart = (int64_t)(seedStart + seedCnt * t);
		info[t].seedEnd = (int64_t)(seedStart + seedCnt * (t + 1));
		info[t].range = range;
		info[t].fullrange = fullrange;
		info[t].filter = filter;
		info[t].withHut = withHut;
		info[t].withMonument = withMonument;
		info[t].minscale = minscale;
		info[t].thread_id = t;
		info[t].genimage = genimage;
	}
	info[threads - 1].seedEnd = seedEnd;

	if (!raw)
		signal(SIGINT, intHandler);

		// start threads
#ifdef USE_PTHREAD

	//if (!raw)
	//{
	pthread_t stats;
	pthread_create(&stats, NULL, statTracker, NULL);
	//}

	for (t = 0; t < threads; t++)
	{
		pthread_create(&threadID[t], NULL, searchCompactBiomesThread, (void *)&info[t]);
	}

	for (t = 0; t < threads; t++)
	{
		pthread_join(threadID[t], NULL);
	}
	exited = 1;

#else

	//if (!raw)
	CreateThread(NULL, 0, statTracker, NULL, 0, NULL);

	for (t = 0; t < threads; t++)
	{
		threadID[t] = CreateThread(NULL, 0, searchCompactBiomesThread, (LPVOID)&info[t], 0, NULL);
	}

	WaitForMultipleObjects(threads, threadID, TRUE, INFINITE);
	exited = 1;

#endif

	if (raw)
		return 0;

	printing = 1;
	fprintf(stderr, "\r%*c", 128, ' ');
	count = 0;
	for (int i = 0; i < 128; i++)
		count += c[i];
	char time_end[20];
	time_t end_time = time(NULL);
	strftime(time_end, 20, "%m/%d/%Y %H:%M:%S", localtime(&end_time));
	printf("\n%20s: %s\n", "Ended", time_end);
	printf("%20s: %ld seconds\n", "Total time elapsed", end_time - start_time);
	printf("%20s: %" PRId64 "\n", "Seeds scanned", count);
	printf("%20s: %li\n", "Viable seeds found", viable_count);
	printf("%20s: %.0f\n", "Average SPS", (double)count / (double)(end_time - start_time));

	printf("\n\nPress [ENTER] to exit\n");
	fflush(stdout);
	getchar();
	return 0;
}
