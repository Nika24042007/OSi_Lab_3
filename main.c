#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <sys/syscall.h>
#include <unistd.h>

typedef struct {
    const double *in;
    double       *out;
    int width, height;
    int row_start, row_end;
    const double *kernel;
    int ksize, krad;
    int show_tid;
    int tid_num; 
} job_m;

static void *worker(void *arg)
{
    job_m *job = (job_m *)arg;
    const int w = job->width, h = job->height;
    const int ksize = job->ksize, krad = job->krad;
    const double *in = job->in, *k = job->kernel;
    double *out = job->out;
    if (job->show_tid) {
        printf("thread %d: TID=%ld, rows [%d, %d)\n",job->tid_num,(long)syscall(SYS_gettid),job->row_start, job->row_end);
    }

    for (int y = job->row_start; y < job->row_end; ++y) {
        for (int x = 0; x < w; ++x) {
            double res = 0.0;
            for (int ky = 0; ky < ksize; ++ky) {
                int sy = y + ky - krad;
                if (sy < 0)  sy = 0;
                if (sy >= h) sy = h - 1;
                for (int kx = 0; kx < ksize; ++kx) {
                    int sx = x + kx - krad;
                    if (sx < 0)  sx = 0;
                    if (sx >= w) sx = w - 1;
                    res += in[(size_t)sy * w + sx] * k[ky * ksize + kx];
                }
            }
            out[(size_t)y * w + x] = res;
        }
    }

    return NULL;
}

static void print_matrix(const char *title, const double *m, int rows, int cols, int show_limit)
{
    printf("\n%s (%dx%d)\n", title, rows, cols);

    int show_r = rows < show_limit ? rows : show_limit;
    int show_c = cols < show_limit ? cols : show_limit;
    int truncated = (show_r < rows) || (show_c < cols);

    if (truncated){
        printf("Show part of matrix %dx%d from %dx%d\n", show_r, show_c, rows, cols);
    }

    for (int y = 0; y < show_r; ++y) {
        for (int x = 0; x < show_c; ++x)
            printf("%8.2f ", m[(size_t)y * cols + x]);
        printf("\n");
    }
}

int main(){
    int rows, cols, ksize, K, max_threads;
    printf("rows cols ksize K max_threads: ");
    if (scanf("%d %d %d %d %d",&rows, &cols, &ksize, &K, &max_threads) != 5) {
        printf("Error enter\n");
        return 1;
    }

    if (rows <= 0 || cols <= 0 || K <= 0 || max_threads <= 0 ||
        ksize <= 0 || ksize % 2 == 0) {
        printf("Error of paramter\n");
        return 1;
    }

    int threads = max_threads;
    if (threads > rows) {
        threads = rows;
    }
    int krad = ksize / 2;

    size_t elems  = (size_t)rows * cols;
    size_t kelems = (size_t)ksize * ksize;

    double *orig   = malloc(elems  * sizeof(double));
    double *a      = malloc(elems  * sizeof(double));
    double *b      = malloc(elems  * sizeof(double));
    double *kernel = malloc(kelems * sizeof(double));
    pthread_t  *ths  = malloc((size_t)threads * sizeof(pthread_t));
    job_m *jobs = malloc((size_t)threads * sizeof(job_m));

    if ((orig == NULL) || (a == NULL) || (b == NULL) ||
        (kernel == NULL) || (ths == NULL) || (jobs == NULL)) {
        printf("Memory error\n");
        perror("malloc");
        return 1;
    }

    srand(42);
    for (size_t i = 0; i < elems; ++i) {
        a[i] = (double)(rand() % 10001) / 100.0;
    }
    memcpy(orig, a, elems * sizeof(double));
    double norm = 1.0 / (double)kelems;
    for (size_t i = 0; i < kelems; ++i) {
        kernel[i] = norm;
    }
    int show_limit = 10;
    if ((rows <= 20) && (cols <= 20)) {
        show_limit = INT_MAX;
    }

    printf("PID: %d, matrix %dx%d, kernel %dx%d, K=%d, threads=%d\n",getpid(), rows, cols, ksize, ksize, K, threads);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int iter = 0; iter < K; ++iter) {
        for (int i = 0; i < threads; ++i) {
            int r0 = (int)(((long long)rows *  i     ) / threads);
            int r1 = (int)(((long long)rows * (i + 1)) / threads);
            jobs[i].in = a;
            jobs[i].out = b;
            jobs[i].width = cols;
            jobs[i].height = rows;
            jobs[i].row_start = r0;
            jobs[i].row_end = r1;
            jobs[i].kernel = kernel;
            jobs[i].ksize = ksize;
            jobs[i].krad = krad;

            jobs[i].show_tid = (iter == 0);
            jobs[i].tid_num  = i;


            if (pthread_create(&ths[i], NULL, worker, &jobs[i]) != 0) {
                perror("pthread_create");
                return 1;
            }
        }
        for (int i = 0; i < threads; ++i) {
            pthread_join(ths[i], NULL);
        }
        double *tmp = a;
        a = b;
        b = tmp;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double time = (double)(t1.tv_sec  - t0.tv_sec)
                   + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;

    print_matrix("Before", orig, rows, cols, show_limit);
    print_matrix("After",   a,    rows, cols, show_limit);

    double sum = 0.0;
    for (size_t i = 0; i < elems; ++i) {
        sum += a[i];
    }
    printf("\nTime: %.6f s, checksum: %.6f\n", time, sum);

    free(orig);
    free(a);
    free(b);
    free(kernel);
    free(ths);
    free(jobs);
    return 0;
}