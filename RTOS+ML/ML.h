/* DEBUG Print */
#ifdef ENERGY
#define PR_DEBUG(fmt, args...) \
    if (0)                     \
    printf(fmt, ##args)
#define PR_INFO(fmt, args...) \
    if (1)                    \
    printf(fmt, ##args)
#else
#define PR_DEBUG(fmt, args...) \
    if (1)                     \
    printf(fmt, ##args)
#define PR_INFO(fmt, args...) \
    if (1)                    \
    printf(fmt, ##args)
#endif

void runModel();
void init_ML();