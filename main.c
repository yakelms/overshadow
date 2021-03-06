#include <stdio.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>     //EXIT_FAILURE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include <sys/times.h>
#include <endian.h>
#ifdef AVX_2
#include <immintrin.h>
/* #include <stdalign.h> */
#endif
#include <pthread.h>
#include <sys/resource.h>

#define NAME_PREFIX "crypt_"
#define PREFIX_LEN strlen(NAME_PREFIX)

#define MAX_WORKERS 64
#define PROCESS_NUM 4

#define OP_ENCRYPT 0x01
#define OP_DECRYPT 0x02

#ifdef AVX_2
#define ALIGN _Alignas(__m256i) //C11
#else
#define ALIGN
#endif

#define WORKER_PROCESS  0
#define WORKER_THREADS  1

int g_workers_num = -1;
int worker_mode = WORKER_PROCESS;

int generate_key(unsigned char *buf, int len)
{
    if (len % sizeof(long))
        return -1;

    //srand48(time(0));
    srandom(time(0));
    int *np = (int *)buf;
    for (int i = 0; i < len / sizeof(int); i++) {
        //long int rand = lrand48();
        long int rand = random();
        *np++ = rand;
        unsigned char *c = (unsigned char *) &rand;
    }

    return 0;
}


int get_output_path (const char *pathname, char *newpath, int size, char flag)
{
    //去掉路径前缀
    int len = 0;
    const char *filename;
    char *slash = strrchr(pathname, '/');
    if (slash) {
        filename = slash + 1;
        len = slash - pathname + 1;
        if (len > size)
            return -1;
        strncpy(newpath, pathname, len);
    } else {
        filename = pathname;
    }

    char newname[NAME_MAX];
    if (OP_DECRYPT == flag) {
        if (strncmp(NAME_PREFIX, filename, PREFIX_LEN)) {
            snprintf(newname, sizeof(newname), "de_%s", filename);
        } else {
            snprintf(newname, sizeof(newname), "%s", filename + PREFIX_LEN);
        }
    } else if (OP_ENCRYPT == flag) {
        snprintf(newname, sizeof(newname), "%s%s", NAME_PREFIX, filename);
    } else {
        return -1;
    }

    len += snprintf(newpath + len, size - len, "%s", newname);

    return len;
}

#ifdef AVX_2
void xor_256i(__m256i *src, __m256i* dst, int data_len, u_int64_t key)
{
    __m256i ymm_key = _mm256_set1_epi64x(key);
    int ymm_nb = data_len / sizeof(__m256i);
    for (int k = 0; k < ymm_nb; k++) {
        __m256i ymm_src = _mm256_loadu_si256(src);
        _mm256_storeu_si256(dst, _mm256_xor_si256(ymm_src, ymm_key));
        src++;
        dst++;
    }
    int left = (data_len % sizeof(__m256i)) / sizeof(u_int64_t);
    u_int64_t *src64 = (u_int64_t *)src;
    u_int64_t *dst64 = (u_int64_t *)dst;
    for (int k = 0; k < left; k++) {
        *dst64++ = *src64++ ^ key;
    }
}

void xor_256i_(u_int64_t *src, u_int64_t *dst, int data_len, u_int64_t key)
{
    if (!src || !dst || !data_len) return;

    static union {
        u_int64_t u64[4];
        __m256i ymm;
    } ymm_mask = {0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff}; 
    static int boot_size = sizeof(__m256i) / sizeof(u_int64_t);
    __m256i ymm_key = _mm256_set1_epi64x(key);
    int ymm_nb = data_len / sizeof(__m256i);
    for (int k = 0; k < ymm_nb; k++) {
        __m256i ymm_src = _mm256_maskload_epi64 ((long long const*)src, ymm_mask.ymm);
        __m256i ymm_dst = _mm256_xor_si256(ymm_src, ymm_key);
        _mm256_maskstore_epi64 ((long long *)dst, ymm_mask.ymm, ymm_dst);
        src += boot_size;
        dst += boot_size;
    }
    int left = (data_len % sizeof(__m256i)) / sizeof(u_int64_t);
    u_int64_t *src64 = (u_int64_t *)src;
    u_int64_t *dst64 = (u_int64_t *)dst;
    for (int k = 0; k < left; k++) {
        *dst64++ = *src64++ ^ key;
    }
}

void encrypt_test() {
    printf("encrypt test\n");
    ALIGN u_int64_t srcs[] = {0xaabbccdd11223344, 0x1232348932129012, 0x1232348932129013,
        0x1232348932129014, 0x1232348932129015, 0x1232348932129016, 0x1232348932129017,
        0x1232348932129018, 0x1232348932129019, 0x1232348932129020, 0x1232348932129021};
    u_int64_t key = 0x23abcd039e126785;
    generate_key((unsigned char *) &key, sizeof(key));
    /* generate_key((unsigned char *) srcs, sizeof(srcs)); */
    ALIGN u_int64_t dsts1[11];
    u_int64_t dsts2[11];

    //encrypt 256
    xor_256i_(srcs, dsts1, sizeof(srcs), key);

    //encrypt 64
    int nb = sizeof(srcs) / sizeof(u_int64_t);
    for (int k = 0; k < nb; k++) {
        dsts2[k] = srcs[k] ^ key;
    }
    printf("encrypt 256:\n");
    int k;
    unsigned char *p;
    p = (unsigned char *)dsts1;
    for (k = 0; k < sizeof(dsts1); k++) {
        printf("%02x", p[k]);
        if ((k+1) % 32 == 0)
            printf("\n");
        else
            printf(" ");
    }
    printf("\nencrypt 64:\n");
    p = (unsigned char *)dsts2;
    for (k = 0; k < sizeof(dsts2); k++) {
        printf("%02x", p[k]);
        if ((k+1) % 32 == 0)
            printf("\n");
        else
            printf(" ");
    }
    if (memcmp(dsts1, dsts2, sizeof(dsts1))) {
        printf("not equal\n");
    }
    else
        printf("equal\n");
    exit(0);
}
#endif

void xor_proc(u_int64_t *src, u_int64_t *dst, int nb, u_int64_t key)
{
#ifdef AVX_2
    /* xor_256i((__m256i *)shadow, (__m256i *)plain, blocks * sizeof(u_int64_t), key); */
    xor_256i_(src, dst, nb * sizeof(u_int64_t), key);
#else
    for (int j = 0; j < nb; j++) {
        *dst++ = *src++ ^ key;
    }
#endif
}

struct thread_info {
    pthread_t tid;
    u_int64_t *src;
    u_int64_t *dst;
    u_int64_t key;
    int blocks;
};

void *thread_xor_proc(void *arg)
{
    struct thread_info *tinfo = (struct thread_info *)arg;
    xor_proc(tinfo->src, tinfo->dst, tinfo->blocks, tinfo->key);
    return NULL;
}

int xor_threads(u_int64_t *src, u_int64_t *dst, int nb, int threads, u_int64_t key, struct thread_info *t)
{
    /* struct thread_info *t = calloc(threads, sizeof(struct thread_info)); */
    /* struct thread_info *t = malloc(threads * sizeof(struct thread_info)); */
    /* if (NULL == t) { */
    /*     perror("malloc"); */
    /*     return 0; */
    /* } */
    /* memset(t, 0, threads * sizeof(struct thread_info)); */
    /* *tids = t; */

    int map_blocks = (nb - 1) / threads;
    int wr_blocks = 0;
    for (int i = 0; i < threads; i++) {
        t[i].src = src + (map_blocks * i);
        t[i].dst = dst + (map_blocks * i);
        t[i].blocks = map_blocks;
        t[i].key = key;
        if (0 != pthread_create(&t[i].tid, NULL, thread_xor_proc, &t[i])) {
            perror("thread_create");
        } else {
            wr_blocks += map_blocks;
        }
    }

    return wr_blocks;
}

int xor_process(u_int64_t *src,
        u_int64_t *dst,
        int nb,
        int workers,
        u_int64_t key,
        /* pid_t **worker_pids) */
        pid_t *pids)
{
    /* pid_t *pids = calloc(workers, sizeof(pid_t)); */
    /* pid_t *pids = malloc(workers * sizeof(pid_t)); */
    /* if (NULL == pids) { */
    /*     perror("malloc"); */
    /*     return 0; */
    /* } */
    /* memset(pids, 0, workers * sizeof(pid_t)); */
    /* *worker_pids = pids; */

    int map_blocks = (nb - 1) / workers;
    int wr_blocks = 0;

    for (int i = 0; i < workers; i++) {
        if (-1 == (pids[i] = fork())) {
            perror("fork");
        } else if (pids[i] == 0) {
            xor_proc(src + (map_blocks * i), dst + (map_blocks * i), map_blocks, key);
            exit(EXIT_SUCCESS);
        } else
            // parent
            wr_blocks += map_blocks;
    }

    return wr_blocks;
}

int xor_worker(u_int64_t *src,
        u_int64_t *dst,
        int nb,
        int workers_nb,
        u_int64_t key,
        void *workers)
{
    int wr_blocks = 0;
    if (worker_mode == WORKER_PROCESS)
        wr_blocks = xor_process(src, dst, nb, workers_nb, key, (pid_t *) workers);
    else
        wr_blocks = xor_threads(src, dst, nb, workers_nb, key, (struct thread_info *) workers);

    return wr_blocks;
}

void wait_workers(int workers_nb, void *workers)
{
    if (!workers) return;

    if (worker_mode == WORKER_PROCESS) {
        pid_t *pids = (pid_t *)workers;
        for (int i = 0; i < workers_nb; i++) {
            if (0 != pids[i]) {
                wait(NULL);
            }
        }
    }
    else {
        struct thread_info *tinfos = (struct thread_info *)workers;
        for (int i = 0; i < workers_nb; i++) {
            if (tinfos[i].tid) {
                pthread_join(tinfos[i].tid, NULL);
            }
        }
    }
    /* free(workers); */
}

int encrypt_file(const char *pathname, const char *outputfile)
{
    int fd;
    if (-1 == (fd = open(pathname, O_RDONLY))) {
        perror(pathname);
        return -1;
    }

    struct stat filestat;
    if (0 != fstat(fd, &filestat)) {
        perror("get file stat error:");
        close(fd);
        return -1;
    }

    long int size = filestat.st_size;
    /* printf("get file %s, size %ld bytes\n", pathname, size); */

    if (!size) {
        close(fd);
        return 0;
    }

    unsigned char encryption_key[sizeof(u_int64_t)];
    if (0 != generate_key(encryption_key, sizeof(encryption_key))) {
        printf("generate_key error\n");
        close(fd);
        return -1;
    }
    u_int64_t key = *(u_int64_t *)encryption_key;

    int e_fd;
    if (!outputfile) {
        char newfile[PATH_MAX];
        if (0 < get_output_path(pathname, newfile, sizeof(newfile), OP_ENCRYPT))
            outputfile = newfile;
        else
            return -1;
    }
    
    if ((e_fd = open(outputfile, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR)) == -1) {
        close(fd);
        perror("create encryption file error:");
        return -1;
    }

    long int end = size % sizeof(u_int64_t);
    long int new_size = size + (end > 0 ? sizeof(u_int64_t) - end : 0);

    //filesize = plain_file_size + key_size + pad_num_block
    long int newfilesize = new_size + sizeof(u_int64_t) + sizeof(u_int64_t);

    //since mmap can't change file size, you must truncate file size first before map it
    if (0 != ftruncate(e_fd, newfilesize)) {
        perror("truncate file failed:");
        close(fd);
        close(e_fd);
        return -1;
    }

#ifdef REALTIME
    struct tms start;
    clock_t realstart = times(&start);
#endif

    void *dst_context = mmap(NULL, newfilesize, PROT_WRITE, MAP_SHARED, e_fd, 0);
    if (MAP_FAILED == dst_context) {
        perror("mmap error:");
        close(fd);
        close(e_fd);
        return -1;
    }

    //write encryption key to head of encryption file, default is 8 bytes.
    u_int64_t *dst = (u_int64_t *)dst_context;
    *dst++ = key;

    int mapflag = MAP_SHARED;
    if (worker_mode == WORKER_THREADS)
        mapflag = MAP_PRIVATE | MAP_POPULATE;
    ALIGN void *context = mmap(NULL, size, PROT_READ, mapflag, fd, 0);
    if (MAP_FAILED == context) {
        perror("mmap error:");
        munmap(dst_context, newfilesize);
        close(fd);
        close(e_fd);
        return -1;
    }

#ifdef REALTIME
    struct tms load;
    clock_t realload = times(&load);
#endif

    int block_num = new_size / sizeof(u_int64_t);

    // only on process is needed if file size is less than 32KB
    int workers_nb = g_workers_num == -1 ? g_workers_num : PROCESS_NUM;
    if (block_num < 4096)
        workers_nb = 0;
    
    int i;
    int wr_blocks = 0;
    u_int64_t *txt = (u_int64_t *)context;

    void *workers = NULL;
    pid_t pids[MAX_WORKERS] = {0};
    struct thread_info tids[MAX_WORKERS] = {0};
    if (worker_mode == WORKER_PROCESS)
        workers = pids;
    else
        workers = tids;

    if (workers_nb) {
        wr_blocks = xor_worker(txt, dst, block_num, workers_nb, key, workers);
        txt += wr_blocks;
        dst += wr_blocks;
    }

    int blocks = (block_num - 1) - wr_blocks;
#ifdef AVX_2
    /* xor_256i((__m256i *)txt, (__m256i *)dst, blocks*sizeof(u_int64_t), key); */
    xor_256i_(txt, dst, blocks*sizeof(u_int64_t), key);
    dst += blocks;
    txt += blocks;
#else
    for (i = 0; i < blocks; i++)
    {
        *dst++ = *txt++ ^ key;
    }
#endif

    /* 明文文件最后一块不足8字节的以0填充后加密写入加密文件 */
    int last_block_bytes = end ? end : sizeof(u_int64_t);
#if 0
    u_int64_t tmp = 0;
    unsigned char *t = (unsigned char *)&tmp;
    unsigned char *ct = (unsigned char *)txt;
    for (i = 0; i < last_block_bytes; i++)
        *t++ = *ct++;
#else
    u_int64_t mask = ((u_int64_t)-1) << ((sizeof(u_int64_t) - last_block_bytes) * 8);
    /*
    #if __BYTE_ORDER == __LITTLE_ENDIAN
    mask = __bswap_64(mask);
    printf("little endian!\n");
    #endif
    */
    u_int64_t tmp = (*txt) & htobe64(mask);
#endif
    *dst++ = tmp ^ key;

    /* 文件最后写入最后一块加密数据的有效字节数，也是加密之后写入 */
    tmp = last_block_bytes;
    *dst = tmp ^ key;

    wait_workers(workers_nb, workers);

#ifdef REALTIME
    struct tms proc;
    clock_t realproc = times(&proc);
#endif

    // unmap and close file here and the data modified for new file is wirtten to the file itself
    munmap(context, size);
    munmap(dst_context, newfilesize);
    close(fd);
    close(e_fd);

#ifdef REALTIME
    struct tms write;
    clock_t realwrite = times(&write);
    if (realstart > 0 && realload > 0 && realproc > 0 && realwrite > 0 ) {
        long clktck = sysconf(_SC_CLK_TCK);
        if (clktck > 0)
            printf("load: %8.3fs\nproc: %8.3fs\nwrite: %8.3fs\n", (realload - realstart)/(double)clktck,
                (realproc - realload)/(double)clktck, (realwrite - realproc)/(double)clktck);
    }
#endif

    printf("encryption file over, file size %ld bytes\n", newfilesize);

    return 0;
}


int decrypt_file(const char *pathname, const char *outputfile)
{
    int fd;
    if (-1 == (fd = open(pathname, O_RDONLY))) {
        perror(pathname);
        return -1;
    }

    //get file size from file information
    struct stat filestat;
    if (0 != fstat(fd, &filestat)) {
        perror("get file stat error:");
        close(fd);
        return -1;
    }
    long int size = filestat.st_size;
    printf("get file %s, size %ld bytes\n", pathname, size);

    if (!size) {
        close(fd);
        return 0;
    }

    if ((size % sizeof(u_int64_t)) || (size < 3 * sizeof(u_int64_t))) {
        printf("invalid encryption file size.\n");
        close(fd);
        return -1;
    }


#ifdef REALTIME
    struct tms start;
    clock_t realstart = times(&start);
#endif

    void *context;
    int mapflag = MAP_SHARED;
    if (worker_mode == WORKER_THREADS)
        mapflag = MAP_PRIVATE | MAP_POPULATE;
    if (MAP_FAILED == (context = mmap(NULL, size, PROT_READ, mapflag, fd, 0))) {
        perror("mmap error:");
        close(fd);
        return -1;
    }

    int newfd;
    if (!outputfile) {
        char newfile[PATH_MAX];
        if (0 < get_output_path(pathname, newfile, sizeof(newfile), OP_DECRYPT))
            outputfile = newfile;
        else
            return -1;
    }

    if ((newfd = open(outputfile, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR)) == -1) {
        munmap(context, size);
        perror("create encryption file error:");
        close(fd);
        return -1;
    }

    int r = size / sizeof(u_int64_t) - 2;       //number of data blocks without key block and last paded block

    u_int64_t key = *(u_int64_t *)context;      //get decryption key from first block
    u_int64_t *txt = (u_int64_t *)context + 1;
    u_int64_t last_block = *(txt + r) ^ key;

    /* 原文件长度等于加密文件长度去掉秘钥块长度，末尾块长度，填充字节长度*/
    long int newfilesize = size - (sizeof(u_int64_t) * 3) + last_block;
    if (0 != ftruncate(newfd, newfilesize)) {
        perror("truncate file failed:");
        close(fd);
        close(newfd);
        munmap(context, size);
        return -1;
    }

    void *dst_context = mmap(NULL, newfilesize, PROT_WRITE, MAP_SHARED, newfd, 0);
    if (MAP_FAILED == dst_context) {
        perror("mmap error:");
        close(fd);
        close(newfd);
        munmap(context, size);
        return -1;
    }


#ifdef REALTIME
    struct tms load;
    clock_t realload = times(&load);
#endif

    u_int64_t *dst = (u_int64_t *)dst_context;
    int workers_nb = g_workers_num == -1 ? g_workers_num : PROCESS_NUM;
    //if encryption file size is litter than 32KB , used 1 process without worker process
    if (r < 4094)
        workers_nb = 0;

    /* pid_t processes_array[workers_nb - 1]; */
    void *workers = NULL;
    pid_t pids[MAX_WORKERS] = {0};
    struct thread_info tids[MAX_WORKERS] = {0};
    if (worker_mode == WORKER_PROCESS)
        workers = pids;
    else
        workers = tids;

    int wr_blocks = 0;
    if (workers_nb) {
        wr_blocks = xor_worker(txt, dst, r, workers_nb, key, workers);
        dst += wr_blocks;
        txt += wr_blocks;
    }

    int left_blocks = r - 1 - wr_blocks;
#ifdef AVX_2
    xor_256i_(txt, dst, left_blocks * sizeof(u_int64_t), key);
    dst += left_blocks;
    txt += left_blocks;
#else
    for (int i = 0; i < left_blocks; i++)
    {
        *dst++ = *txt++ ^ key;
    }
#endif

    // mmap 映射长度为内存页大小的整数倍，这里可以直接整个块写入，保存文件会自动按文件长度截断
#if 0
    int write_bytes = sizeof(u_int64_t);
    if (last_block) {
        write_bytes = last_block;
    }

    u_int64_t decryption = *txt ^ key;
    memcpy(dst, &decryption, write_bytes);
#else
    *dst = *txt ^ key;
#endif

    wait_workers(workers_nb, workers);

#ifdef REALTIME
    struct tms proc;
    clock_t realproc = times(&proc);
#endif

    munmap(dst_context, newfilesize);
    munmap(context, size);
    close(fd);
    close(newfd);

#ifdef REALTIME
    struct tms write;
    clock_t realwrite = times(&write);
    if (realstart > 0 && realload > 0 && realproc > 0 && realwrite > 0 ) {
        long clktck = sysconf(_SC_CLK_TCK);
        if (clktck > 0)
            printf("load: %8.3fs\nproc: %8.3fs\nwrite: %8.3fs\n", (realload - realstart)/(double)clktck,
                (realproc - realload)/(double)clktck, (realwrite - realproc)/(double)clktck);
    }
#endif

    printf("decryption file over, file size %ld bytes.\n", newfilesize);

    return 0;
}

#define usage(app) do { \
    printf("Usage: %s -{e|d|h} [-n <workers_numb>] [-m p[rocess]|t[hread] -i <inputfile> [-o outputfile]\n", (app)); \
    exit(EXIT_FAILURE); \
}while(0);

#define ENCRYPT 0x01
#define DECRYPT 0x02

void print_times(clock_t real, struct tms *start, struct tms *end)
{
    long clktck = sysconf(_SC_CLK_TCK);
    if (clktck < 0) {
        perror("sysconf(_SC_CLK_TCK):");
        return;
    }

    printf("real:%8.3fs\nuser:%8.3fs\nsys:%8.3fs\nclild user:%8.3fs\nchild sys:%8.3fs\n",
            real/(double)clktck, (end->tms_utime - start->tms_utime)/(double)clktck,
            (end->tms_stime - start->tms_stime)/(double)clktck,
            (end->tms_cutime - start->tms_cutime)/(double)clktck,
            (end->tms_cstime - start->tms_cstime)/(double)clktck);
}

#define pr_limit(resource) print_limit(#resource, resource)
void print_limit(char *name, int resource)
{
    struct rlimit rlim;
    if (0 > getrlimit(RLIMIT_AS, &rlim)) {
        perror("getrlimit(RLIMIT_AS):");
    } else {
        printf("%-14s\t", name);
        if (rlim.rlim_cur == RLIM_INFINITY) 
            printf("cur[INFINITY]\t");
        else
            printf("cur[%10ld]\t", rlim.rlim_cur);
        
        if (rlim.rlim_max == RLIM_INFINITY)
            printf("max[INFINITY]\n");
        else
            printf("max[%10ld]\n", rlim.rlim_max);
    }
}

#define TIMES(real, tms) { if (-1 == (real = times(&tms))) { perror("times"); exit(EXIT_FAILURE);}}
int main(int argc, char **argv)
{
#ifdef AVX_2
    /* encrypt_test(); */
#endif

    if (argc < 3) {
        usage(argv[0]);
    }

#ifdef DEBUG
    pr_limit(RLIMIT_AS);
#endif

    struct tms start;
    clock_t realstart;
    TIMES(realstart, start);

    int qid, opt;
    int mode = 0; /* 0 init, 1 encrypt, 2 decrypt */
    int msgType = 1;
    int msgKey = -1;

    struct option longopts[] = {
        {"encrypt", no_argument, NULL, 'e'},
        {"decrypt", no_argument, NULL, 'd'},
        {"workers_number", required_argument, NULL, 'n'},
        {"workers_mode", required_argument, NULL, 'm'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int msgflag = 0600;
    char *inputfile = NULL, *outputfile = NULL;
    while ((opt = getopt_long(argc, argv, "edn:hi:o:m:", longopts, &option_index)) != -1) {
        switch (opt) {
            case 'e':
                mode |= ENCRYPT;
                break;
            case 'd':
                mode |= DECRYPT;
                break;
            case 'n':
                g_workers_num = strtol(optarg, NULL, 10); 
                if (g_workers_num > MAX_WORKERS) {
                    fprintf(stderr, "workers's nubmer must less than %d\n", MAX_WORKERS);
                    usage(argv[0]);
                }
                break;
            case 'i':
                inputfile = optarg;
                break;
            case 'o':
                outputfile = optarg;
                break;
            case 'm':
                if ('t' == optarg[0])
                    worker_mode = WORKER_THREADS;
                else if ('p' == optarg[0])
                    worker_mode = WORKER_PROCESS;
                else
                    usage(argv[0]);
                break;
            case 'h':
            default:
                usage(argv[0]);
        }
    }

    //set stdout and seterr unbuffered
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    int rlt;
    if (mode == ENCRYPT) {
        rlt = encrypt_file(inputfile, outputfile);
    } else if (mode == DECRYPT) {
        rlt = decrypt_file(inputfile, outputfile);
    } else {
        usage(argv[0]);
    }

    struct tms end;
    clock_t realend;
    TIMES(realend, end);
    print_times(realend-realstart, &start, &end);

    if (0 != rlt) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
