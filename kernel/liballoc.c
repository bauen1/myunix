/*
Modified version, original version at https://github.com/blanham/liballoc

This code is released into the public domain. Use this code at your own
risk. Feel free to use it for whatever purpose you want. I take no responsibilty or
whatever if anything goes wrong.  Use it at your own risk.

If you have any fixes or patches, please email me.

Durand Miller <clutter@djm.co.za>
*/

#include <assert.h>

#include <console.h>
#include <cpu.h>
#include <heap.h>
#include <pmm.h>
#include <vmm.h>
#include <string.h>

#define LIBALLOC_MAGIC 0xc001c0de
#define MAXCOMPLETE 5
#define MAXEXP 32
#define MINEXP 8

#define MODE_BEST 0
#define MODE_INSTANT 1

#define MODE MODE_BEST

/** This is a boundary tag which is prepended to the
 * page or section of a page which we have allocated. It is
 * used to identify valid memory blocks that the
 * application is trying to free.
 */
struct	boundary_tag
{
	unsigned int magic;               // magic value
	unsigned int size;                // requested size
	unsigned int real_size;	          // actual size.
	int index;                        // location in the page table.

	struct boundary_tag *split_left;  // linked-list info for broken pages.
	struct boundary_tag *split_right; // linked-list info for broken pages.

	struct boundary_tag *next;        // linked-list info.
	struct boundary_tag *prev;        // linked-list info.
};

// allowing for 2^MAXEXP blocks
struct boundary_tag * l_freePages[MAXEXP];
int l_completePages[MAXEXP];

#ifdef DEBUG
// real amount of memory allocated
unsigned int l_allocated = 0;
// the amount of memory in use
unsigned int l_inuse = 0;
#endif

// flag to indicate initializtion.
static int l_initialized = 0;
// minimum number of pages to allocate
static unsigned int l_pageCount = 16; // FIXME: replace with #define ?

// ***********   HELPER FUNCTIONS  *******************************

/** This function is supposed to lock the memory data structures. It
 * could be as simple as disabling interrupts or acquiring a spinlock.
 * It's up to you to decide.
 *
 * \return 0 if the lock was acquired successfully. Anything else is
 * failure.
 */

static int eflags;

static int liballoc_lock() {
	// TODO: implement properly
	__asm__ __volatile__("pushf\n"
				"pop %0\n"
				"cli"
			: "=r" (eflags));
	return 0;
}

/** This function unlocks what was previously locked by the liballoc_lock
 * function.  If it disabled interrupts, it enables interrupts. If it
 * had acquiried a spinlock, it releases the spinlock. etc.
 *
 * \return 0 if the lock was successfully released.
 */
static int liballoc_unlock() {
	// TODO: implement properly
	if (eflags & (1 << 9)) {
		__asm__ __volatile__("sti");
	}
	return 0;
}

/** This is the hook into the local system which allocates pages. It
 * accepts an integer parameter which is the number of pages
 * required.  The page size was set up in the liballoc_init function.
 *
 * \return NULL if the pages were not allocated.
 * \return A pointer to the allocated memory.
 */
static int liballoc_free(void *v, size_t pages);
static void *liballoc_alloc(size_t pages) {
	uintptr_t v_start = find_vspace(kernel_directory, pages);
	for (size_t i = 0; i < pages; i++) {
		uintptr_t real_block = pmm_alloc_blocks(1);
		if (real_block == 0) {
			printf("OOM!!\n");
			if (i > 0) {
				liballoc_free((void *)v_start, i - 1);
			}
			halt();
			return NULL;
		}
		map_page(get_table_alloc(v_start + i * BLOCK_SIZE, kernel_directory),
			v_start + i * BLOCK_SIZE, real_block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
	}
	return (void *)v_start;
}

/** This frees previously allocated memory. The void* parameter passed
 * to the function is the exact same value returned from a previous
 * liballoc_alloc call.
 *
 * The integer value is the number of pages to free.
 *
 * \return 0 if the memory was successfully freed.
 */
static int liballoc_free(void *v, size_t pages) {
	assert(v != NULL); // unnecessary ?
	for (size_t i = 0; i < pages; i++) {
		uintptr_t vaddr = (uintptr_t)v + i * BLOCK_SIZE;
		page_table_t *table = get_table(vaddr, kernel_directory);
		assert(table != NULL);
		page_t page = get_page(table, vaddr);
		uintptr_t block = page & ~0xFFF;
		map_page(table, vaddr, 0, 0);
		pmm_free_blocks(block, 1);
	}
	return 0;
}


/** Returns the exponent required to manage 'size' amount of memory.
 *
 *  Returns n where  2^n <= size < 2^(n+1)
 */
static inline int getexp( unsigned int size )
{
	if ( size < (1<<MINEXP) ) {
		#ifdef DEBUG
		printf("getexp returns -1 for %i less than MINEXP\n", size );
		#endif
		return -1; // Smaller than the quantum.
	}

	unsigned int shift = MINEXP;

	while ( shift < MAXEXP )
	{
		if ( (unsigned)(1<<shift) > size) {
			break;
		}
		shift += 1;
	}

	#ifdef DEBUG
	printf("getexp returns %i (%i bytes) for %i size\n", shift - 1, (1<<(shift -1)), size );
	#endif

	return shift - 1;
}

#ifdef DEBUG
static void dump_array()
{
	int i = 0;
	struct boundary_tag *tag = NULL;

	printf("------ Free pages array ---------\n");
	printf("System memory allocated: %i\n", l_allocated );
	printf("Memory in used (malloc'ed): %i\n", l_inuse );

		for ( i = 0; i < MAXEXP; i++ )
		{
			printf("%.2i(%i): ",i, l_completePages[i] );

			tag = l_freePages[ i ];
			while ( tag != NULL )
			{
				if ( tag->split_left  != NULL  ) {
					printf("*");
				}
				printf("%i", tag->real_size );
				if ( tag->split_right != NULL  ) {
					printf("*");
				}

				printf(" ");
				tag = tag->next;
			}
			printf("\n");
		}

	printf("'*' denotes a split to the left/right of a tag\n");
}
#endif

static inline void insert_tag( struct boundary_tag *tag, int index )
{
	int realIndex;

	if ( index < 0 ) {
		realIndex = getexp( tag->real_size - sizeof(struct boundary_tag) );
		if ( realIndex < MINEXP ) {
			realIndex = MINEXP;
		}
	} else {
		realIndex = index;
	}
	tag->index = realIndex;

	if ( l_freePages[ realIndex ] != NULL ) {
		l_freePages[ realIndex ]->prev = tag;
		tag->next = l_freePages[ realIndex ];
	}

	l_freePages[ realIndex ] = tag;
}

static inline void remove_tag( struct boundary_tag *tag )
{
	if ( l_freePages[ tag->index ] == tag ) {
		l_freePages[ tag->index ] = tag->next;
	}

	if ( tag->prev != NULL ) {
		tag->prev->next = tag->next;
	}
	if ( tag->next != NULL ) {
		tag->next->prev = tag->prev;
	}

	tag->next = NULL;
	tag->prev = NULL;
	tag->index = -1;
}

static inline struct boundary_tag* melt_left( struct boundary_tag *tag )
{
	struct boundary_tag *left = tag->split_left;

	left->real_size   += tag->real_size;
	left->split_right  = tag->split_right;

	if ( tag->split_right != NULL ) {
		tag->split_right->split_left = left;
	}

	return left;
}


static inline struct boundary_tag* absorb_right( struct boundary_tag *tag )
{
	struct boundary_tag *right = tag->split_right;

	remove_tag( right );		// Remove right from free pages.
	tag->real_size   += right->real_size;
	tag->split_right  = right->split_right;
	if ( right->split_right != NULL ) {
		right->split_right->split_left = tag;
	}

	return tag;
}

static inline struct boundary_tag* split_tag( struct boundary_tag* tag )
{
	unsigned int remainder = tag->real_size - sizeof(struct boundary_tag) - tag->size;

	struct boundary_tag *new_tag =
		(struct boundary_tag*)((unsigned int)tag + sizeof(struct boundary_tag) + tag->size);

	new_tag->magic = LIBALLOC_MAGIC;
	new_tag->real_size = remainder;

	new_tag->next = NULL;
	new_tag->prev = NULL;

	new_tag->split_left = tag;
	new_tag->split_right = tag->split_right;

	if (new_tag->split_right != NULL) {
		new_tag->split_right->split_left = new_tag;
	}

	tag->split_right = new_tag;

	tag->real_size -= new_tag->real_size;

	insert_tag( new_tag, -1 );

	return new_tag;
}

// ***************************************************************

static struct boundary_tag* allocate_new_tag( unsigned int size )
{
	unsigned int pages;
	unsigned int usage;
	struct boundary_tag *tag;

	// This is how much space is required.
	usage  = size + sizeof(struct boundary_tag);

	// Perfect amount of space
	pages = usage / PAGE_SIZE;
	if ( (usage % PAGE_SIZE) != 0 ) {
		pages += 1;
	}

	// Make sure it's >= the minimum size.
	if ( pages < l_pageCount ) {
		pages = l_pageCount;
	}

	tag = (struct boundary_tag*)liballoc_alloc( pages );

	if ( tag == NULL ) {
		return NULL;	// uh oh, we ran out of memory.
	}

	tag->magic 		= LIBALLOC_MAGIC;
	tag->size 		= size;
	tag->real_size 	= pages * PAGE_SIZE;
	tag->index 		= -1;

	tag->next		= NULL;
	tag->prev		= NULL;
	tag->split_left 	= NULL;
	tag->split_right 	= NULL;


	#ifdef DEBUG
	printf("Resource allocated %x of %i pages (%i bytes) for %i size.\n", tag, pages, pages * PAGE_SIZE, size );

	l_allocated += pages * PAGE_SIZE;

	printf("Total memory usage = %i KB\n",  (int)((l_allocated / (1024))) );
	#endif
	return tag;
}

void liballoc_init(void) {
	#ifdef DEBUG
	printf("%s\n","liballoc initializing.");
	#endif
	for (unsigned int index = 0; index < MAXEXP; index++ ) {
		l_freePages[index] = NULL;
		l_completePages[index] = 0;
	}

	l_initialized = 1;
}

void * __attribute__((malloc)) kmalloc(size_t size)
{
	int index;
	void *ptr;
	struct boundary_tag *tag = NULL;

	assert(l_initialized != 0);

	assert(liballoc_lock() == 0);

	index = getexp( size ) + MODE;
	if ( index < MINEXP ) {
		index = MINEXP;
	}

	// Find one big enough.
	// Start at the front of the list.
	tag = l_freePages[ index ];
	while ( tag != NULL ) {
		// If there's enough space in this tag.
		if ( (tag->real_size - sizeof(struct boundary_tag))
			>= (size + sizeof(struct boundary_tag) ) )
		{
			#ifdef DEBUG
			printf("Tag search found %i >= %i\n",(tag->real_size - sizeof(struct boundary_tag)), (size + sizeof(struct boundary_tag) ) );
			#endif
			break;
		}

		tag = tag->next;
	}

	// No page found. Make one.
	if ( tag == NULL ) {
		if ( (tag = allocate_new_tag( size )) == NULL ) {
			assert(liballoc_unlock() == 0);
			assert(0);
			return NULL;
		}
		index = getexp( tag->real_size - sizeof(struct boundary_tag) );
	} else {
		remove_tag( tag );
		if ( (tag->split_left == NULL) && (tag->split_right == NULL) ) {
			l_completePages[ index ] -= 1;
		}
	}

	// We have a free page.  Remove it from the free pages list.
	tag->size = size;
	// Removed... see if we can re-use the excess space.

	#ifdef DEBUG
	printf("Found tag with %i bytes available (requested %i bytes, leaving %i), which has exponent: %i (%i bytes)\n", tag->real_size - sizeof(struct boundary_tag), size, tag->real_size - size - sizeof(struct boundary_tag), index, 1<<index );
	#endif

	unsigned int remainder = tag->real_size - size - sizeof( struct boundary_tag ) * 2; // Support a new tag + remainder

	if ( ((int)(remainder) > 0) /*&& ( (tag->real_size - remainder) >= (1<<MINEXP))*/ )
	{
		int childIndex = getexp( remainder );

		if ( childIndex >= 0 )
		{
			#ifdef DEBUG
			printf("Seems to be splittable: %i >= 2^%i .. %i\n", remainder, childIndex, (1<<childIndex) );
			struct boundary_tag *new_tag = split_tag( tag );
			printf("Old tag has become %i bytes, new tag is now %i bytes (%i exp)\n", tag->real_size, new_tag->real_size, new_tag->index );
			#else
			split_tag(tag);
			#endif
		}
	}

	ptr = (void*)((unsigned int)tag + sizeof( struct boundary_tag ) );

	#ifdef DEBUG
	l_inuse += size;
	printf("malloc: %x,  %i, %i\n", ptr, (int)l_inuse / 1024, (int)l_allocated / 1024 );
	dump_array();
	#endif

	assert(liballoc_unlock() == 0);
	return ptr;
}

void kfree(void *ptr)
{
	int index;
	struct boundary_tag *tag;

	if ( ptr == NULL ) {
		return;
	}

	assert(liballoc_lock() == 0);

	tag = (struct boundary_tag*)((unsigned int)ptr - sizeof( struct boundary_tag ));

	if ( tag->magic != LIBALLOC_MAGIC ) {
		assert(liballoc_unlock() == 0);
		return;
	}

	#ifdef DEBUG
	l_inuse -= tag->size;
	printf("free: %x, %i, %i\n", ptr, (int)l_inuse / 1024, (int)l_allocated / 1024 );
	#endif

	// MELT LEFT...
	while ( (tag->split_left != NULL) && (tag->split_left->index >= 0) ) {
		#ifdef DEBUG
		printf("Melting tag left into available memory. Left was %i, becomes %i (%i)\n", tag->split_left->real_size, tag->split_left->real_size + tag->real_size, tag->split_left->real_size );
		#endif
		tag = melt_left( tag );
		remove_tag( tag );
	}

	// MELT RIGHT...
	while ( (tag->split_right != NULL) && (tag->split_right->index >= 0) ) {
		#ifdef DEBUG
		printf("Melting tag right into available memory. This was was %i, becomes %i (%i)\n", tag->real_size, tag->split_right->real_size + tag->real_size, tag->split_right->real_size );
		#endif
		tag = absorb_right( tag );
	}

	// Where is it going back to?
	index = getexp( tag->real_size - sizeof(struct boundary_tag) );
	if ( index < MINEXP ) index = MINEXP;

	// A whole, empty block?
	if ( (tag->split_left == NULL) && (tag->split_right == NULL) ) {
		if ( l_completePages[ index ] == MAXCOMPLETE ) {
			// Too many standing by to keep. Free this one.
			unsigned int pages = tag->real_size / PAGE_SIZE;

			if ( (tag->real_size % PAGE_SIZE) != 0 ) pages += 1;
			if ( pages < l_pageCount ) pages = l_pageCount;

			liballoc_free( tag, pages );

			#ifdef DEBUG
			l_allocated -= pages * PAGE_SIZE;
			printf("Resource freeing %x of %i pages\n", tag, pages );
			dump_array();
			#endif

			assert(liballoc_unlock() == 0);
			return;
		}

		l_completePages[ index ] += 1;	// Increase the count of complete pages.
	}

	// ..........

	insert_tag( tag, index );

	#ifdef DEBUG
	printf("Returning tag with %i bytes (requested %i bytes), which has exponent: %i\n", tag->real_size, tag->size, index );
	dump_array();
	#endif

	assert(liballoc_unlock() == 0);
}

void * __attribute__((malloc)) kcalloc(size_t nobj, size_t size) {
       int real_size;
       void *p;

       real_size = nobj * size;
       p = kmalloc( real_size );

       memset( p, 0, real_size );

       return p;
}

void* krealloc(void *p, size_t size) {
	void *ptr;
	struct boundary_tag *tag;
	size_t real_size;
	if ( size == 0 )
	{
		kfree( p );
		return NULL;
	}
	if ( p == NULL ) return kmalloc( size );

	assert(liballoc_lock() == 0);

	tag = (struct boundary_tag*)((unsigned int)p - sizeof( struct boundary_tag ));
	real_size = tag->size;

	assert(liballoc_unlock() == 0);

	if ( real_size > size ) {
		real_size = size;
	}

	ptr = kmalloc( size );
	memcpy( ptr, p, real_size );
	kfree( p );

	return ptr;
}
