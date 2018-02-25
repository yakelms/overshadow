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

/* get file pathname and generate a 16 btypes key to encrypt file. */

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

        //printf("%ld , 0x%02x%02x%02x%02x\n", rand, c[0], c[1], c[2], c[3]);
    }

    /* for debug 
    for (int j = 0; j < len; j++)
        printf("%02x", buf[j]);
    printf("\n");
    */

    return 0;
}
int decryption_file(unsigned char *key, int key_size, int fd);
#define NAME_PREFIX "crypt_"
#define PREFIX_LEN strlen(NAME_PREFIX)

#define PROCESS_NUM 4
int encrypt_file(const char *pathname)
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
    printf("get file %s, size %ldB\n", pathname, size);

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
    char newfile[NAME_MAX];
    snprintf(newfile, NAME_MAX, NAME_PREFIX"%s", pathname);
    if ((e_fd = open(newfile, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR)) == -1) {
        close(fd);
        perror("create encryption file error:");
        return -1;
    }

    long int end = size % sizeof(u_int64_t);
    long int new_size = size + (end > 0 ? sizeof(u_int64_t) - end : 0);

    long int newfilesize = new_size + sizeof(u_int64_t) + sizeof(u_int64_t);
    //mmap 映射不能增加文件长度，必须先增加文件长度，在映射写入文件内容
    ftruncate(e_fd, newfilesize);

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

    void *context = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (MAP_FAILED == context) {
        perror("mmap error:");
        munmap(dst_context, newfilesize);
        close(fd);
        close(e_fd);
        return -1;
    }

    int block_num = new_size / sizeof(u_int64_t);
    int map_blocks = block_num / PROCESS_NUM;
    
    pid_t processes_array[PROCESS_NUM - 1];
    int i;
    int wr_blocks = 0;
    for (i = 0; i < PROCESS_NUM - 1; i++) {
        processes_array[i] = fork();
        if (-1 == processes_array[i])
            perror("fork");
        else if (0 == processes_array[i]) {
            u_int64_t *txt = (u_int64_t *)context + (map_blocks * i);
            u_int64_t *map = dst + (map_blocks * i);
            int j;
            for (j = 0; j < map_blocks; j++)
                *map++ = *txt++ ^ key;
            munmap(context, size);
            munmap(dst_context, newfilesize);
            close(fd);
            close(e_fd);
            return 0;
        }
        else
            wr_blocks += map_blocks;
    }

    //encryption file context and write to encryption file.
    //int wr_blocks = map_blocks * (PROCESS_NUM - 1);
    int blocks = block_num - wr_blocks - 1;
    printf("block_num=%d, map_blocks=%d, wr_blocks=%d, blocks=%d\n",
        block_num, map_blocks, wr_blocks, blocks);

    u_int64_t *txt = (u_int64_t *)context + wr_blocks;
    dst += wr_blocks;
    for (i = 0; i < blocks; i++)
    {
        *dst++ = *txt++ ^ key;
    }

    /* 明文文件最后一块不足8字节的以0填充后加密写入加密文件 */
    u_int64_t tmp = 0;
    unsigned char *t = (unsigned char *)&tmp;
    unsigned char *ct = (unsigned char *)txt;
    for (i = 0; i < end; i++)
        *t++ = *ct++;
    *dst++ = tmp ^ key;

    /* 文件最后写入最后一块加密数据的有效字节数，也是加密之后写入 */
    tmp = end;

    *dst = tmp ^ key;

    for (i = 0; i < PROCESS_NUM - 1; i++) {
        if (-1 != processes_array[i]) {
            int wstatus;
            waitpid(processes_array[i], &wstatus, 0);    
        }
    }

    munmap(context, size);
    munmap(dst_context, newfilesize);
    close(fd);
    close(e_fd);
    printf("encryption file over\n");
    
    return 0;
}

//int decrypt_file(unsigned char *decryption_key, int key_size, int fd)
int decrypt_file(const char *pathname)
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
    printf("get file %s, size %ldB\n", pathname, size);

    if (!size) {
        close(fd);
        return 0;
    }

    if ((size % sizeof(u_int64_t)) || (size < 3 * sizeof(u_int64_t))) {
        printf("invalid encryption file size.\n");
        close(fd);
        return -1;
    }

    void *context;
    if (MAP_FAILED == (context = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0))) {
        perror("mmap error:");
        close(fd);
        return -1;
    }

    int newfd;
    const char *newfile = pathname + PREFIX_LEN;

    if ((newfd = open(newfile, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR)) == -1) {
        munmap(context, size);
        perror("create encryption file error:");
        close(fd);
        return -1;
    }

    int r = size / sizeof(u_int64_t) - 1;       //去掉秘钥

    u_int64_t key = *(u_int64_t *)context;      //get decryption key from first block
    u_int64_t *txt = (u_int64_t *)context + 1;
    u_int64_t last_block = *(txt + r - 1) ^ key;
    printf("get last block as %ld\n", last_block);

    /* 原文件长度等于加密文件长度去掉秘钥块长度，末尾块长度，填充字节长度*/
    long int newfilesize = size - (sizeof(u_int64_t) * 3) + last_block;
    //mmap 映射不能增加文件长度，必须先增加文件长度，在映射写入文件内容
    ftruncate(newfd, newfilesize);

    void *dst_context = mmap(NULL, newfilesize, PROT_WRITE, MAP_SHARED, newfd, 0);
    if (MAP_FAILED == dst_context) {
        perror("mmap error:");
        close(fd);
        close(newfd);
        munmap(context, size);
        return -1;
    }

    u_int64_t *dst = (u_int64_t *)dst_context;
    pid_t processes_array[PROCESS_NUM - 1];
    int map_blocks = (r - 1) / PROCESS_NUM;
    int i;
    int wr_blocks = 0;
    for (i = 0; i < PROCESS_NUM - 1; i++) {
        processes_array[i] = fork();
        if (-1 == processes_array[i])
            perror("fork");
        else if (0 == processes_array[i]) {
            u_int64_t *t = txt + (map_blocks * i);
            u_int64_t *map = dst + (map_blocks * i);
            int j;
            for (j = 0; j < map_blocks; j++)
                *map++ = *t++ ^ key;
            munmap(context, size);
            munmap(dst_context, newfilesize);
            close(fd);
            close(newfd);
            return 0;
        }
        else
            wr_blocks += map_blocks;
    }

    int left_blocks = r - 2 - wr_blocks;
    dst += wr_blocks;
    txt += wr_blocks;
    for (i = 0; i < left_blocks; i++)
    {
        *dst++ = *txt++ ^ key;
    }

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

    munmap(dst_context, newfilesize);
    close(newfd);
    munmap(context, size);
    close(fd);
    printf("decryption file size %ld.\n", newfilesize);

    return 0;
}

#define usage(app) do { printf("Usage: %s -{e|d} <pathname>\n", (app)); exit(EXIT_FAILURE);}while(0);
#define ENCRYPT 0x01
#define DECRYPT 0x02

int main(int argc, char **argv)
{
    if (argc != 3) {
        usage(argv[0]);
    }

    int qid, opt;
    int mode = 0; /* 0 init, 1 encrypt, 2 decrypt */
    int msgType = 1;
    int msgKey = -1;

    struct option longopts[] = {
        {"encrypt", required_argument, NULL, 'e'},
        {"decrypt", required_argument, NULL, 'd'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int msgflag = 0600;
    while ((opt = getopt_long(argc, argv, "e:d:", longopts, &option_index)) != -1) {
        switch (opt) {
            case 'e':
                mode |= ENCRYPT;
                break;
            case 'd':
                mode |= DECRYPT;
                break;
            default:
                usage(argv[0]);
        }
    }

    int rlt;
    if (mode == ENCRYPT) {
        rlt = encrypt_file(argv[2]);
    } else if (mode == DECRYPT) {
        rlt = decrypt_file(argv[2]);
    } else {
        usage(argv[0]);
    }

    if (0 != rlt) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
