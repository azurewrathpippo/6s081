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
  struct spinlock lock;
  struct buf buf[NBUF];

  struct spinlock bucket_lock[NBUCKET];
  struct buf uncached_buf;
  struct buf *bucket[NBUCKET];// hold all cached buffer
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

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKET; i++) {
    char buffer[32];
    snprintf(buffer, 32, "bcache[%d]", i);
    initlock(&bcache.bucket_lock[i], buffer);
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    
    b->next = bcache.uncached_buf.next;
    bcache.uncached_buf.next = b;
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
    struct buf *b = bcache.bucket[bucket_index];
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
  acquire(&bcache.lock);
  if (bcache.uncached_buf.next) {
    struct buf *b = bcache.uncached_buf.next;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    bcache.uncached_buf.next = b->next;
    release(&bcache.lock);

    // put buf into hash table.
    b->next = bcache.bucket[bucket_index];
    bcache.bucket[bucket_index] = b;
    b->refcnt = 1;
    release(&bcache.bucket_lock[bucket_index]);

    acquiresleep(&b->lock);
    acquire(&tickslock);
    b->ts = ticks;
    release(&tickslock);
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
  if (b->refcnt == 0) {
    acquire(&bcache.lock);
    // remove b from hash table
    if (bcache.bucket[bucket_index] == b) {
      bcache.bucket[bucket_index] = b->next;
    } else {
      struct buf *prev = bcache.bucket[bucket_index];
      while (prev->next != b) {
        prev = prev->next;
      }
      prev->next = b->next;
    }
    // put b back into unused buffer
    b->next = bcache.uncached_buf.next;
    bcache.uncached_buf.next = b;
    release(&bcache.lock);
  }
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


