// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
  // struct spinlock lock;
  struct buf buf[NBUF];

  struct spinlock bucket_lock[NBUCKET];
  struct buf bucket[NBUCKET];// hold all cached buffer
} bcache;

static int
hash_func(uint dev, uint blockno) {
  uint64 h = dev;
  h <<= 32;
  h |= blockno;
  return h % NBUCKET;
}

void
binit(void)
{
  struct buf *b;

  // initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKET; i++) {
    static char buffer[NBUCKET][64];
    snprintf(buffer[i], 64, "bcache.bucket[%d]", i);
    initlock(&bcache.bucket_lock[i], buffer[i]);
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");

    // place buf into a bucket.
    int bucket_index = hash_func(0, b - bcache.buf);
    b->next = bcache.bucket[bucket_index].next;
    bcache.bucket[bucket_index].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int bucket_index = hash_func(dev, blockno);
  acquire(&bcache.bucket_lock[bucket_index]);

  // Is the block already cached?
  {
    struct buf *b = bcache.bucket[bucket_index].next;
    while (b) {
      if (b->dev == dev && b->blockno == blockno) {
        b->refcnt++;
        release(&bcache.bucket_lock[bucket_index]);
        acquiresleep(&b->lock);
        return b;
      }
      b = b->next;
    }
  }

  // Not cached.
  // find one empty buffer.
  release(&bcache.bucket_lock[bucket_index]);
  struct buf *unused = 0;

  for (int i = 0; i < NBUCKET; i++) {
    int searching_bucket_index = (bucket_index + i) % NBUCKET;
    acquire(&bcache.bucket_lock[searching_bucket_index]);

    struct buf *b = &bcache.bucket[searching_bucket_index];
    while (b->next) {
      if (b->next->refcnt == 0) {
        unused = b->next;
        b->next = b->next->next;
        release(&bcache.bucket_lock[searching_bucket_index]);
        break;
      }
      b = b->next;
    }
    if (unused) {
      break;
    }
    release(&bcache.bucket_lock[searching_bucket_index]);
  }

  if (unused) {
    struct buf *b = unused;
    acquire(&bcache.bucket_lock[bucket_index]);

    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;

    // put buf into hash table.
    b->next = bcache.bucket[bucket_index].next;
    bcache.bucket[bucket_index].next = b;
    release(&bcache.bucket_lock[bucket_index]);

    acquiresleep(&b->lock);
    return b;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket_index = hash_func(b->dev, b->blockno);
  acquire(&bcache.bucket_lock[bucket_index]);
  b->refcnt--;
  release(&bcache.bucket_lock[bucket_index]);
}

void
bpin(struct buf *b) {
  int bucket_index = hash_func(b->dev, b->blockno);
  acquire(&bcache.bucket_lock[bucket_index]);
  b->refcnt++;
  release(&bcache.bucket_lock[bucket_index]);
}

void
bunpin(struct buf *b) {
  int bucket_index = hash_func(b->dev, b->blockno);
  acquire(&bcache.bucket_lock[bucket_index]);
  b->refcnt--;
  release(&bcache.bucket_lock[bucket_index]);
}


