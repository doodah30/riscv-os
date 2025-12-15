#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf head;
} bcache;

void binit(void) {
  struct buf *b;
  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
static struct buf* bget(uint dev, uint blockno) {
  struct buf *b;

  acquire(&bcache.lock);
  // === 【调试代码 1】 ===
  printf("BGET: acquiring bcache.lock for dev %d, block %d\n", dev, blockno);
  // =====================
  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    // === 【调试代码 2】 ===
    printf("BGET: checking cached buf %p (dev %d, block %d). Valid=%d, Refcnt=%d\n", b, b->dev, b->blockno, b->valid, b->refcnt);
    // =====================
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      // === 【调试代码 3】 ===
      printf("BGET: found in cache: buf %p\n", b);
      // =====================
      printf("BGET: found free/LRU buf %p. About to acquire its sleep lock (lk->locked=%d)...\n", b, b->lock.locked);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // === 【调试代码 4】 ===
  printf("BGET: not in cache, looking for free/LRU buf...\n");
  // =====================
  // Not cached; recycle an LRU buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    // === 【调试代码 5】 ===
    printf("BGET: checking LRU buf %p (dev %d, block %d). Refcnt=%d\n", b, b->dev, b->blockno, b->refcnt);
    // =====================
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      // === 【调试代码 6】 ===
      printf("BGET: found free/LRU buf %p. Acquiring its sleep lock...\n", b);
      // =====================
      printf("BGET: found free/LRU buf %p. About to acquire its sleep lock (lk->locked=%d)...\n", b, b->lock.locked);
      acquiresleep(&b->lock);
      // === 【调试代码 7】 ===
      printf("BGET: acquired buf sleep lock.\n");
      // =====================
      return b;
    }
  }
  // === 【调试代码 8】 ===
  printf("BGET: no buffers available, panicking!\n");
  // =====================
  panic("bget: no buffers");
  return 0;
}

// Return a locked buf with the contents of the indicated block.
struct buf* bread(uint dev, uint blockno) {
  struct buf *b;
  b = bget(dev, blockno);
  // === 【调试代码】 ===
  printf("BREAD: bget returned buf %p for dev %d, block %d. Valid=%d\n", b, dev, blockno, b->valid);
  // ====================
  if(!b->valid) {
    // === 【调试代码】 ===
    printf("BREAD: buf not valid, calling virtio_disk_rw to read...\n");
    // ====================
    virtio_disk_rw(b, 0); // read
    b->valid = 1;
    // === 【调试代码】 ===
    printf("BREAD: virtio_disk_rw returned.\n");
    // ====================
  }
  return b;
}

// Write b's contents to disk.
void bwrite(struct buf *b) {
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1); // write
}

// Release a locked buffer.
void brelse(struct buf *b) {
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}