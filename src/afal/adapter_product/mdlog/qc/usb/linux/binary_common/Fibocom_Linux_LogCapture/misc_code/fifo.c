#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fifo.h"

#define is_power_of_2(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))

#define min(a, b) (((a) < (b)) ? (a) : (b))

static unsigned int roundup_pow_of_two(unsigned int val)
{
    if((val & (val - 1)) == 0) {
        return val;
    }

    unsigned int tmp = (unsigned int)((unsigned int)~0);
    unsigned int andv = ~(tmp&(tmp >> 1));

    while((andv & val) == 0)
        andv = andv>>1;

    return andv<<1;
}

static struct fifo *fifo_init(unsigned int size)
{
    struct fifo *fifo = NULL;

    /* size must be a power of 2 */
    //BUG_ON(!is_power_of_2(size));

    fifo = malloc(sizeof(struct fifo));
    if (!fifo)
        return NULL;

    fifo->buffer = malloc(size);
    if (!fifo->buffer) {
        free(fifo);
        return NULL;
    }

    fifo->size = size;
    fifo->in = fifo->out = 0;
    //fifo->lock = lock;

    return fifo;
}

struct fifo *fifo_alloc(unsigned int size)
{
    /*
     * round up to the next power of 2, since our 'let the indices
     * wrap' technique works only in this case.
     */
    if (!is_power_of_2(size)) {
        //BUG_ON(size > 0x80000000);
        size = roundup_pow_of_two(size);
    }

    return fifo_init(size);
}

void fifo_free(struct fifo *fifo)
{
    if (fifo) {
        if (fifo->buffer) {
            free(fifo->buffer);
            fifo->buffer = NULL;
        }

        free(fifo);
        fifo = NULL;
    }
}

unsigned int __fifo_put(struct fifo *fifo, const unsigned char *buffer, unsigned int len)
{
    unsigned int l;

    len = min(len, fifo->size - fifo->in + fifo->out);

    /*
     * Ensure that we sample the fifo->out index -before- we
     * start putting bytes into the fifo.
     */
    //smp_mb();
    __sync_synchronize();

    /* first put the data starting from fifo->in to buffer end */
    l = min(len, fifo->size - (fifo->in & (fifo->size - 1)));
    memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buffer, l);

    /* then put the rest (if any) at the beginning of the buffer */
    memcpy(fifo->buffer, buffer + l, len - l);

    /*
     * Ensure that we add the bytes to the fifo -before-
     * we update the fifo->in index.
     */

    //smp_wmb();
    __sync_synchronize();

    fifo->in += len;

    return len;
}

unsigned int __fifo_get(struct fifo *fifo, unsigned char *buffer, unsigned int len)
{
    unsigned int l;

    len = min(len, fifo->in - fifo->out);

    /*
     * Ensure that we sample the fifo->in index -before- we
     * start removing bytes from the fifo.
     */

    //smp_rmb();
    __sync_synchronize();

    /* first get the data from fifo->out until the end of the buffer */
    l = min(len, fifo->size - (fifo->out & (fifo->size - 1)));
    memcpy(buffer, fifo->buffer + (fifo->out & (fifo->size - 1)), l);

    /* then get the rest (if any) from the beginning of the buffer */
    memcpy(buffer + l, fifo->buffer, len - l);

    /*
     * Ensure that we remove the bytes from the fifo -before-
     * we update the fifo->out index.
     */
    //smp_mb();
    __sync_synchronize();

    fifo->out += len;

    return len;
}
