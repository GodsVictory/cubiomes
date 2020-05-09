#include "finders.h"
#include "generator.h"
#include "math.h"
#include <time.h>

struct compactinfo_t
{
    int64_t seedStart, seedEnd;
    unsigned int range;
    unsigned int fullrange;
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
int printing = 0;
int exited = 0;
int exit_counter = 0;
int raw = 0;

#ifdef USE_PTHREAD
static void *statTracker()
#else
static DWORD WINAPI statTracker(LPVOID lpParameter)
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
        sps = count / (this_time - start_time);
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
        fflush(stderr);
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
                    if (abs(p.x) < info.fullrange && abs(p.z) < info.fullrange)
                    {
                        if (isViableFeaturePos(Swamp_Hut, g, cache, p.x, p.z))
                        {
                            huts[hut_count] = p;
                            hut_count++;
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
                                        huts_found = 1;
                                    }
                                    if (huts_found)
                                        break;
                                }
                                if (huts_found)
                                    break;
                            }
                        }
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
                    if (abs(p.x) < info.fullrange && abs(p.z) < info.fullrange)
                        if (isViableOceanMonumentPos(g, cache, p.x, p.z))
                            monument_count++;
                    if (monument_count > 0)
                        break;
                }
                if (monument_count > 0)
                    break;
            }
            if (monument_count == 0)
                continue;
        }

        // biome enum defined in layers.h
        enum BiomeID req_biomes[] = {ice_spikes, bamboo_jungle, desert, plains, ocean, jungle, forest, mushroom_fields, mesa, flower_forest, warm_ocean, frozen_ocean, megaTaiga, roofedForest, extremeHills, swamp, savanna, icePlains};
        int biome_exists[sizeof(req_biomes) / sizeof(enum BiomeID)] = {0};

        int r = info.fullrange;
        for (z = -r; z < r; z += step)
        {
            for (x = -r; x < r; x += step)
            {
                Pos p = {x, z};
                int biome = getBiomeAtPos(g, p);
                if (abs(x) < info.range && abs(z) < info.range)
                {
                    for (int i = 0; i < sizeof(biome_exists) / sizeof(int); i++)
                    {
                        if (biome == req_biomes[i])
                            biome_exists[i] = -1;
                        if (req_biomes[i] != -1)
                            continue;
                        break;
                    }
                }
            }
        }

        //verify all biomes are present
        int all_biomes = 1;
        for (int i = 0; i < sizeof(req_biomes) / sizeof(enum BiomeID); i++)
            if (biome_exists[i] != -1)
                all_biomes = 0;
        if (!all_biomes)
            continue;

        viable_count++;

        fprintf(stderr, "\r%*c", 128, ' ');
        printf("\r%" PRId64 "\n", s);
        fflush(stdout);
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

    enum BiomeID biomes[] = {ice_spikes, bamboo_jungle, desert, plains, ocean, jungle, forest, mushroom_fields, mesa, flower_forest, warm_ocean, frozen_ocean, megaTaiga, roofedForest, extremeHills, swamp, savanna, icePlains};
    filter = setupBiomeFilter((const int *)biomes,
                              sizeof(biomes) / sizeof(enum BiomeID));
    minscale = 256; // terminate search at this layer scale
    // TODO: simple structure filter
    withHut = 1;
    withMonument = 1;
    start_time = time(NULL);

    thread_id_t *threadID = malloc(threads * sizeof(thread_id_t));
    struct compactinfo_t *info = malloc(threads * sizeof(struct compactinfo_t));

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
    }
    info[threads - 1].seedEnd = seedEnd;

    // start threads
#ifdef USE_PTHREAD

    pthread_t stats;
    pthread_create(&stats, NULL, statTracker, NULL);

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

    CreateThread(NULL, 0, statTracker, NULL, 0, NULL);

    for (t = 0; t < threads; t++)
    {
        threadID[t] = CreateThread(NULL, 0, searchCompactBiomesThread, (LPVOID)&info[t], 0, NULL);
    }

    WaitForMultipleObjects(threads, threadID, TRUE, INFINITE);
    exited = 1;

#endif

    free(info);
    free(threadID);

    return 0;
}
