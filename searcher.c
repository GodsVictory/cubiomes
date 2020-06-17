#include "finders.h"
#include "generator.h"
#include "math.h"

#include <time.h>
#include <unistd.h>

struct compactinfo_t
{
    int64_t seedEnd;
    int64_t batchStart;
    int64_t batchSize;
    int64_t* currentSeed;
    unsigned int range;
    unsigned int fullrange;
    BiomeFilter filter;
    int withHut, withMonument;
    int minscale;
    int thread_id;

    int step;
};

int64_t currentSeed;

int64_t c[128] = {0};
long viable_count = 0;

char eta[20];
time_t start_time;
int64_t total_seeds = 0;

float global_step = 8;
int exited = 0;

//These are not used
float max_ocean = 25; //maximum amount of ocean allowed in percentage
float min_major_biomes = 0; //minimum major biome percent
//int raw = 0;
//int exit_counter = 0;
//int printing = 0;

#ifdef USE_PTHREAD
static void *statTracker()
#else
static DWORD WINAPI statTracker(LPVOID lpParameter)
#endif
{
    int64_t count = 0;
//    long last_viable_count = 0;
//    int64_t last_count = 0;
    float sps = 0;

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
//        last_count = count;
//        last_viable_count = viable_count;

        //Sleep, otherwise this thread costs as much as the worker threads.
        sleep(1);
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

    // biome enum defined in layers.h
    enum BiomeID req_biomes[] = {
            ice_spikes, bamboo_jungle, desert, plains, ocean, jungle, forest,
            mushroom_fields, mesa, flower_forest, warm_ocean, frozen_ocean,
            megaTaiga, roofedForest, extremeHills, swamp, savanna, icePlains};
    int biome_exists[sizeof(req_biomes) / sizeof(enum BiomeID)] = {0};
    int biomeExistsLength = sizeof(biome_exists) / sizeof(int);
    int biomeIdToIndexTable[LAST_BIOME];
    int uniqueBiomes = sizeof(req_biomes) / sizeof(enum BiomeID);

    int i;
    for (i = 0; i < (sizeof(biomeIdToIndexTable) / sizeof(int)); i++)
    {
        biomeIdToIndexTable[i] = -1;
    }

    for (i = 0; i < biomeExistsLength; i++)
    {
        biomeIdToIndexTable[req_biomes[i]] = i;
    }

    int ax = -info.range, az = -info.range;
    int w = 2 * info.range, h = 2 * info.range;
    int64_t s;

    LayerStack* g = setupGenerator(MC_1_15);
    int *cache = allocCache(&g->layers[L_VORONOI_ZOOM_1], w, h);
    int *cache2 = allocCache(&g->layers[L_VORONOI_ZOOM_1], 1, 1);
    Pos huts[100];

    int64_t currentBatchSize = info.batchSize;
    int64_t batchStart = info.batchStart;
    int64_t batchStop = batchStart + currentBatchSize;

    while (batchStart < info.seedEnd)
    {

        for (s = batchStart; s < batchStop; s++)
        {
            c[info.thread_id]++;
            if (!checkForBiomes(g, cache, s, ax, az, w, h, info.filter, info.minscale)) {
                continue;
            }
            applySeed(g, s);
            int x, z;

            if (info.withHut) {
                int hut_count = 0;

                int huts_found = 0;
                int r = info.fullrange / SWAMP_HUT_CONFIG.regionSize;
                for (z = -r; z < r; z++)
                {
                    for (x = -r; x < r; x++)
                    {
                        Pos p;
                        p = getStructurePos(SWAMP_HUT_CONFIG, s, x, z);
                        if (abs(p.x) < info.fullrange && abs(p.z) < info.fullrange
                                && isViableFeaturePos(Swamp_Hut, g, cache, p.x, p.z)) {
                            huts[hut_count] = p;
                            hut_count++;
                            if (hut_count < 2) {
                                continue;
                            }
                            int last_hut = hut_count - 1;
                            for (int i = 0; i < last_hut; i++)
                            {
                                int dx, dz;
                                dx = huts[i].x - huts[last_hut].x;
                                dz = huts[i].z - huts[last_hut].z;
                                if ((dx * dx) + (dz * dz) <= 40000) {
                                    huts_found = 1;
                                    goto afterHutsCheck;
                                }
                            }
                        }
                    }
                }
                if (!huts_found) {
                    continue;
                }
            }
            afterHutsCheck:

            if (info.withMonument) {
                int monument_count = 0;
                int r = info.fullrange / MONUMENT_CONFIG.regionSize;
                for (z = -r; z < r; z++)
                {
                    for (x = -r; x < r; x++)
                    {
                        Pos p;
                        p = getLargeStructurePos(MONUMENT_CONFIG, s, x, z);
                        if (abs(p.x) < info.fullrange && abs(p.z) < info.fullrange
                            && isViableOceanMonumentPos(g, cache, p.x, p.z)) {
                            monument_count++;
                            goto afterMonumentCheck;
                        }
                    }
                }
                if (monument_count == 0) {
                    continue;
                }
            }
            afterMonumentCheck:

            memset(&biome_exists, 0, sizeof(biome_exists));
            int biomeCounter = 0;

            // Unnecessary int cast removes warning in darwin build
            int negative = abs((int) (info.range - info.fullrange) % info.step) - info.range;
            int positive = info.range - info.step + abs((int) (info.range - info.fullrange) % info.step);

            for (z = negative; z <= positive; z += info.step)
            {
                for (x = negative; x <= positive; x += info.step)
                {
                    int biome = getBiomeAtPosWithCache(g, x, z, cache2);
                    int biomeInfo = biomeIdToIndexTable[biome];
                    if (biomeInfo == -1 || biome_exists[biomeInfo]) {
                        continue;
                    }
                    biome_exists[biomeInfo] = -1;
                    ++biomeCounter;
                    if (biomeCounter == uniqueBiomes) {
                        goto afterBiomeCheck;
                    }
                }
            }

            if (biomeCounter < uniqueBiomes) {
                continue;
            }
            afterBiomeCheck:

            viable_count++;

            fprintf(stderr, "\r%*c", 128, ' ');
            fprintf(stdout, "\r%" PRId64 "\n", s);
            fflush(stdout);
        }

        batchStart = __atomic_fetch_add(info.currentSeed, currentBatchSize, __ATOMIC_RELAXED);
        batchStop = batchStart + currentBatchSize;

        if (batchStop > info.seedEnd) {
            batchStop = info.seedEnd;
        }
    }

    freeGenerator(g);
    free(cache);
    free(cache2);

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
    if (seedStart == 0 && seedEnd == -1) {
        seedStart = -999999999999999999;
        seedEnd = 999999999999999999;
    }

    currentSeed = seedStart;

    int64_t* currentSeedAddress = &currentSeed;

    total_seeds = (uint64_t)seedEnd - (uint64_t)seedStart;
    //uint64_t seedCnt = ((uint64_t)seedEnd - (uint64_t)seedStart) / threads;
    uint64_t batchSize = (total_seeds / (threads * 10));
    if (batchSize == 0) {
        batchSize = 1;
    } else if (batchSize > 10000000) {
        batchSize = 10000000;
    }

    for (t = 0; t < threads; t++)
    {
        info[t].currentSeed = currentSeedAddress;
        info[t].batchSize = batchSize;
        info[t].batchStart = seedStart + batchSize * t;
        info[t].seedEnd = seedEnd;

        info[t].range = range;
        info[t].fullrange = fullrange;

        info[t].filter = filter;
        info[t].withHut = withHut;
        info[t].withMonument = withMonument;
        info[t].minscale = minscale;
        info[t].step = global_step;

        info[t].thread_id = t;
    }
    currentSeed = currentSeed + batchSize * threads;


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

#else

    CreateThread(NULL, 0, statTracker, NULL, 0, NULL);

    for (t = 0; t < threads; t++)
    {
        threadID[t] = CreateThread(NULL, 0, searchCompactBiomesThread, (LPVOID)&info[t], 0, NULL);
    }

    WaitForMultipleObjects(threads, threadID, TRUE, INFINITE);
    exited = 1;

#endif

    exited = 1;

    free(info);
    free(threadID);

    return 0;
}
