#ifndef __FIFO_H__
#define __FIFO_H__

#ifdef __cplusplus
extern "C" {
#endif

struct fifo {
    unsigned char *buffer;  /* the data buffer */
    unsigned int size;  /* the size of the allocated buffer */
    unsigned int in;    /* data is added at offset (in % size) */
    unsigned int out;   /* data is extracted from off. (out % size) */
    //spinlock_t *lock; /* protects concurrent modifications */
};

/**
 * fifo_alloc - alloc a FIFO
 * @buffer:  cache pointer.
 *
 * This function init a fifo struct using malloc space
 */
struct fifo *fifo_alloc(unsigned int size);


/**
 * fifo_free - init a FIFO
 * @fifo: the fifo to be used.
 *
 * This function will free cache
 */
void fifo_free(struct fifo *fifo);


/**
 * fifo_put - puts some data into the FIFO
 * @fifo: the fifo to be used.
 * @buffer: the data to be added.
 * @len: the length of the data to be added.
 *
 * This function copies at most @len bytes from the @buffer into
 * the FIFO depending on the free space, and returns the number of
 * bytes copied.
 */
unsigned int __fifo_put(struct fifo *fifo, const unsigned char *buffer, unsigned int len);


/**
 * __fifo_get - gets some data out of the FIFO
 * @fifo: the fifo to be used.
 * @buffer: the data to be geted.
 * @len: the length of the data to be geted.
 *
 * This function copies at most @len bytes from the @fifo to @buffer and
 * returns the number of bytes copied.
 */
unsigned int __fifo_get(struct fifo *fifo, unsigned char *buffer, unsigned int len);

/**
 * __fifo_len - returns the number of bytes available in the FIFO, no locking version
 * @fifo: the fifo to be used.
 */
static inline unsigned int __fifo_len(struct fifo *fifo)
{
    return fifo->in - fifo->out;
}

/**
 * __fifo_reset - removes the entire FIFO contents, no locking version
 * @fifo: the fifo to be emptied.
 */
static inline void __fifo_reset(struct fifo *fifo)
{
    fifo->in = fifo->out = 0;
}

#ifdef __cplusplus
}
#endif

#endif
