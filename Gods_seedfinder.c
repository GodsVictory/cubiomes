#include "finders.h"
#include "generator.h"
#include "math.h"
#include <time.h>
#include <signal.h>

struct compactinfo_t
{
	int64_t seedStart, seedEnd;
	unsigned int range;
	BiomeFilter filter;
	int withHut, withMonument;
	int minscale;
	int thread_id;
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
time_t last_time;
int exited = 0;
int exit_counter = 0;
void intHandler()
{
	exited = 1;
	printf("\n");
	if (exit_counter++)
		exit(0);
	char time_end[20];
	time_t end_time = time(NULL);
	strftime(time_end, 20, "%m/%d/%Y %H:%M:%S", localtime(&end_time));
	printf("\n%20s: %s\n", "Ended", time_end);
	printf("%20s: %ld seconds\n", "Total time elapsed", end_time - start_time);
	printf("%20s: %lli\n", "Seeds scanned", count);
	printf("%20s: %li\n", "Viable seeds found", viable_count);
	printf("%20s: %.0f\n", "Average SPS", (double)count / (double)(end_time - start_time));
	printf("\n\nPress [ENTER] to exit\n");
	getchar();
	exit(0);
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
	int64_t s;

	LayerStack g = setupGenerator(MC_1_15);
	int *cache = allocCache(&g.layers[L_VORONOI_ZOOM_1], w, h);

	last_time = time(NULL);
	for (s = info.seedStart; s != info.seedEnd; s++)
	{
		if (exited)
			break;
		c[info.thread_id]++;
		time_t this_time = time(NULL);
		if (this_time - last_time >= 5 || viable_count > last_viable_count)
		{
			count = 0;
			for (int i = 0; i < 128; i++)
				count += c[i];
			sps = (count - last_count) / (this_time - last_time);
			time_t predict_end = this_time + (double)total_seeds / sps;
			strftime(eta, 20, "%H:%M:%S", localtime(&predict_end));
			float percent_done = (double)count / (double)total_seeds * 100;
			if (percent_done < 0)
				percent_done = 0;
			long int seconds_passed = this_time - start_time;
			float eta = (double)(total_seeds - count) / sps;
			if (eta < 0 || percent_done < 0)
				fprintf(stderr, "\rscanned: %10lli | viable: %3li | sps: %5.0lf | elapsed: %7.0lds", count, viable_count, sps, seconds_passed);
			else
				fprintf(stderr, "\rscanned: %10lli | viable: %3li | sps: %5.0lf | elapsed: %7.0lds | %3.2lf%% | eta: %7.0fs  ", count, viable_count, sps, seconds_passed, percent_done, eta);
			fflush(stdout);
			last_time = this_time;
			last_count = count;
			last_viable_count = viable_count;
		}
		if (!checkForBiomes(&g, cache, s, ax, az, w, h, info.filter, info.minscale))
			continue;
		passed_filter++;
		applySeed(&g, s);
		int x, z;
		int r = info.range;

		if (info.withMonument)
		{
			int monument_found = 0;
			int r = info.range / MONUMENT_CONFIG.regionSize;
			for (z = -r; z < r; z++)
			{
				for (x = -r; x < r; x++)
				{
					Pos p;
					p = getLargeStructurePos(MONUMENT_CONFIG, s, x, z);
					if (isViableOceanMonumentPos(g, cache, p.x, p.z))
						if (abs(p.x) < info.range && abs(p.z) < info.range)
							monument_found = 1;
					if (monument_found)
						break;
				}
				if (monument_found)
					break;
			}
			if (!monument_found)
				continue;
		}

		Pos goodhuts[2];
		if (info.withHut)
		{
			Pos huts[100];
			int counter = 0;
			int r = info.range / SWAMP_HUT_CONFIG.regionSize;
			for (z = -r; z < r; z++)
			{
				for (x = -r; x < r; x++)
				{
					Pos p;
					p = getStructurePos(SWAMP_HUT_CONFIG, s, x, z);
					if (isViableFeaturePos(Swamp_Hut, g, cache, p.x, p.z))
					{
						if (abs(p.x) < info.range && abs(p.z) < info.range)
						{
							huts[counter] = p;
							counter++;
							//printf("%i\n", huts[0].x);
						}
					}
				}
			}

			int huts_found = 0;
			for (int i = 0; i < counter; i++)
			{
				for (int j = 0; j < counter; j++)
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
					if (huts_found)
						break;
				}
				if (huts_found)
					break;
			}
			if (!huts_found)
				continue;
		}

		float ocean_count = 0;
		// biome enum defined in layers.h
		enum BiomeID biomes[] = {ice_spikes, bamboo_jungle, desert, plains, ocean, jungle, forest, mushroom_fields, mesa, flower_forest, warm_ocean, frozen_ocean, megaTaiga, roofedForest, extremeHills, swamp, savanna, icePlains};

		enum BiomeID major_biomes[] = {desert, plains, jungle, forest, roofedForest, mesa, extremeHills, swamp, savanna, icePlains};
		int major_biome_counter[sizeof(major_biomes) / sizeof(enum BiomeID)] = {0};

		for (z = -r; z < r; z += step)
		{
			for (x = -r; x < r; x += step)
			{
				Pos p = {x, z};
				int biome = getBiomeAtPos(g, p);
				if (isOceanic(biome))
					ocean_count++;
				for (int i = 0; i < sizeof(major_biome_counter) / sizeof(int); i++)
					if (biome == major_biomes[i])
						major_biome_counter[i]++;
				for (int i = 0; i < sizeof(biomes) / sizeof(enum BiomeID); i++)
					if (biome == biomes[i])
						biomes[i] = -1;
			}
		}

		//check for max ocean percent
		float ocean_percent = (ocean_count * (step * step) / (w * h)) * 100;
		if (ocean_percent > max_ocean)
			continue;

		//check for minimum major biome percent
		int major_biome_less_than_min = 1;
		for (int i = 0; i < sizeof(major_biome_counter) / sizeof(int); i++)
			if ((major_biome_counter[i] * (step * step) / (w * h)) * 100 < min_major_biomes)
				major_biome_less_than_min = 0;
		if (!major_biome_less_than_min)
			continue;

		//verify all biomes are present
		int all_biomes = 1;
		for (int i = 0; i < sizeof(biomes) / sizeof(enum BiomeID); i++)
			if (biomes[i] != -1)
				all_biomes = 0;
		if (!all_biomes)
			continue;

		viable_count++;

		char *major_biomes_string[] = {"desert", "plains", "jungle", "forest", "roofedForest", "mesa", "extremeHills", "swamp", "savanna", "icePlains"};

		fprintf(stderr, "\r%*c", 100, ' ');
		printf("\n%15s: %" PRId64 "\n", "Found", s);
		printf("%15s: %d,%i & %i,%i\n", "Huts", goodhuts[0].x, goodhuts[0].z, goodhuts[1].x, goodhuts[1].z);
		printf("%15s: %.2f%%\n", "Ocean", ocean_percent);
		for (int i = 0; i < sizeof(major_biome_counter) / sizeof(int); i++)
			printf("%15s: %.2f%%\n", major_biomes_string[i], (major_biome_counter[i] * (step * step) / (w * h)) * 100);
		printf("\n");
		fflush(stdout);

		FILE *fp;
		if (!(fp = fopen("found.csv", "r")))
		{
			fp = fopen("found.csv", "a");
			fprintf(fp, "Seed,Huts,Ocean");
			for (int i = 0; i < sizeof(major_biome_counter) / sizeof(int); i++)
				fprintf(fp, ",%s", major_biomes_string[i]);
			fprintf(fp, "\n");
		}
		else
			fp = fopen("found.csv", "a");
		fprintf(fp, "%" PRId64 "", s);
		fprintf(fp, ",%i:%i & %i:%i", goodhuts[0].x, goodhuts[0].z, goodhuts[1].x, goodhuts[1].z);
		fprintf(fp, ",%.2f%%", ocean_percent);
		for (int i = 0; i < sizeof(major_biome_counter) / sizeof(int); i++)
			fprintf(fp, ",%.2f%%", (major_biome_counter[i] * (step * step) / (w * h)) * 100);
		fprintf(fp, "\n");
		fclose(fp);
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

	start_time = time(NULL);
	char time_start[20];
	strftime(time_start, 20, "%m/%d/%Y %H:%M:%S", localtime(&start_time));
	printf("Started: %s\n", time_start);

	int64_t seedStart, seedEnd;
	unsigned int threads, t, range;
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
		printf("Radius: ");
		if (!scanf("%i", &range))
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
	withHut = 1;
	withMonument = 1;
	total_seeds = (uint64_t)seedEnd - (uint64_t)seedStart;

	printf("Starting search through seeds %" PRId64 " to %" PRId64 ", using %u threads.\n"
		   "Search radius = %u.\n",
		   seedStart, seedEnd, threads, range);

	thread_id_t threadID[threads];
	struct compactinfo_t info[threads];

	// store thread information
	uint64_t seedCnt = ((uint64_t)seedEnd - (uint64_t)seedStart) / threads;
	for (t = 0; t < threads; t++)
	{
		info[t].seedStart = (int64_t)(seedStart + seedCnt * t);
		info[t].seedEnd = (int64_t)(seedStart + seedCnt * (t + 1));
		info[t].range = range;
		info[t].filter = filter;
		info[t].withHut = withHut;
		info[t].withMonument = withMonument;
		info[t].minscale = minscale;
		info[t].thread_id = t;
	}
	info[threads - 1].seedEnd = seedEnd;

	// start threads
#ifdef USE_PTHREAD

	for (t = 0; t < threads; t++)
	{
		pthread_create(&threadID[t], NULL, searchCompactBiomesThread, (void *)&info[t]);
	}

	for (t = 0; t < threads; t++)
	{
		pthread_join(threadID[t], NULL);
	}

#else

	for (t = 0; t < threads; t++)
	{
		threadID[t] = CreateThread(NULL, 0, searchCompactBiomesThread, (LPVOID)&info[t], 0, NULL);
	}

	signal(SIGINT, intHandler);

	WaitForMultipleObjects(threads, threadID, TRUE, INFINITE);

#endif

	fprintf(stderr, "\r%*c", 100, ' ');
	count = 0;
	for (int i = 0; i < 128; i++)
		count += c[i];
	char time_end[20];
	time_t end_time = time(NULL);
	strftime(time_end, 20, "%m/%d/%Y %H:%M:%S", localtime(&end_time));
	printf("\n%20s: %s\n", "Ended", time_end);
	printf("%20s: %ld seconds\n", "Total time elapsed", end_time - start_time);
	printf("%20s: %lli\n", "Seeds scanned", count);
	printf("%20s: %li\n", "Viable seeds found", viable_count);
	printf("%20s: %.0f\n", "Average SPS", (float)count / (float)(end_time - start_time));

	return 0;
}
