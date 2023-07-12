/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20191564",
    /* Your full name*/
    "Donghun Ko",
    /* Your email address */
    "rhehdtor@sogang.ac.kr",
};


/* my malloc */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<9)

#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp)-DSIZE)))

/* for explicit list */
#define PFRBP(bp) (*(void**)(bp)) // prev block 의 bp 를 가져옴
#define NFRBP(bp) (*(void**)((bp) + WSIZE))  // next block 의 bp 를 포인터

static char *heap_listp;
static void *first_list = NULL; // free list 의 첫번째 원소를 가리키고 있음

static void *extend_heap(size_t words);
static void* coalesce(void* bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);

static void DeleteFreeBlock(void* bp);
static void InsertFreeBlock(void* bp, size_t size);
/* 
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void*)-1) return -1;

    PUT(heap_listp, PACK(0,1));
    PUT(heap_listp+1*WSIZE, PACK(2*DSIZE, 1)); // prologue
    PUT(heap_listp+2*WSIZE, 0);  // initial prev = null
    PUT(heap_listp+3*WSIZE, 0);  // initial next = null
    PUT(heap_listp+4*WSIZE, PACK(2*DSIZE, 1)); // prologue
    PUT(heap_listp+5*WSIZE, PACK(0, 1));
    heap_listp += (2*WSIZE);
    first_list = heap_listp;

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    
    return 0;
}

static void* coalesce(void* bp){
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case1. Prev, Next 모두 할당 */
    if(prev_alloc && next_alloc){
        InsertFreeBlock(bp, size);
    }

    /* Case2. Prev는 할당, Next는 Free */
    else if(prev_alloc && !next_alloc){
        void* next_bp = NEXT_BLKP(bp);
        size += GET_SIZE(HDRP(next_bp));
        DeleteFreeBlock(next_bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        InsertFreeBlock(bp, size);
    }

    /* Case3. Prev는 Free, Next는 할당 */
    else if(!prev_alloc && next_alloc){
        void* prev_bp = PREV_BLKP(bp);
        size += GET_SIZE(HDRP(prev_bp));
        DeleteFreeBlock(prev_bp);

        bp = prev_bp;

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        InsertFreeBlock(bp, size);
    }

    /* Case4. Prev, Next 모두 Free */
    else{
        void* next_bp = NEXT_BLKP(bp);
        void* prev_bp = PREV_BLKP(bp);
        size += GET_SIZE(FTRP(next_bp)) + GET_SIZE(HDRP(prev_bp));
        DeleteFreeBlock(next_bp);
        DeleteFreeBlock(prev_bp);
        bp = prev_bp;

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        InsertFreeBlock(bp, size);
    }

    return bp;
}

/* mm_init 후 | heap 영역이 부족해지면 호출 */
static void *extend_heap(size_t words){
    char* bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));  // flag for epilogue

    return coalesce(bp);
}

// free list를 순회하면서, 적당한 size의 block 이 있는지 확인
static void* find_fit(size_t asize){
    void* trav;
    
    // next가 0이 아닐 때까지 순회
    for(trav=first_list; NFRBP(trav) != 0; trav = NFRBP(trav)){
        // 충분히 할당할 수 있을 경우
        if(GET_SIZE(HDRP(trav)) >= asize){
            return trav;
        }
    }
    return NULL;
}

// bp 가 가리키는 곳에 있는 free block 을 제거해줌
static void DeleteFreeBlock(void* bp){

    // free list의 첫번째 node인 경우, first_list를 바꿔줘야 함
    if(bp == first_list){
        void* nptr = NFRBP(bp); // next free block의 block pointer 값을 얻어옴
        PFRBP(nptr) = 0; // prev 값을 0으로 만들어서 첫번째 node 로 바꿔줌
        first_list = nptr; // 첫번째 노드를 이걸로 바꿔줌
    }

    // 중간에 들어있는 node인 경우. prev, next 바꿔줌
    else{
        void* pptr = PFRBP(bp);
        void* nptr = NFRBP(bp);
        // prev's next = cur's next
        NFRBP(pptr) = NFRBP(bp);
        // next's prev = cur's prev
        PFRBP(nptr) = PFRBP(bp);
    }
}

static void InsertFreeBlock(void* bp, size_t size){

    void*trav;

    for(trav = first_list; NFRBP(trav) != 0; trav = NFRBP(trav)){
        // 현재 보고 있는 Block의 사이즈가 넣고자 하는애보다 큰 경우, 이 앞에 삽입. 오름차순이 되도록
        if(size <= GET_SIZE(trav)){
            // 첫 block 인 경우, 맨 앞으로 삽입
            if(PFRBP(trav) == 0){
                PFRBP(bp) = 0;
                NFRBP(bp) = trav;
                PFRBP(trav) = bp;
                first_list = bp;
            }

            // 첫 block이 아닌 경우, 사이에 삽입
            else{
                void*prev_bp = PFRBP(trav);

                NFRBP(bp) = trav;
                PFRBP(bp) = prev_bp;
                NFRBP(prev_bp) = bp;
                PFRBP(trav) = bp; 
            }
            return;
        }
    }

    // 해당 block 의 사이즈가 현존하는 block 중 가장 큰 것일 경우, initial block 앞에다가 삽입.
    if(PFRBP(trav) == 0){
        PFRBP(bp) = 0;
        NFRBP(bp) = trav;
        PFRBP(trav) = bp;
        first_list = bp;
    }

    // 첫 block이 아닌 경우, 사이에 삽입
    else{
        void*prev_bp = PFRBP(trav);
        NFRBP(bp) = trav;
        PFRBP(bp) = prev_bp;
        NFRBP(prev_bp) = bp;
        PFRBP(trav) = bp; 
    }
}

// bp에 asize 만큼 할당 
static void place(void* bp, size_t asize){
    // 할당
    size_t csize = GET_SIZE(HDRP(bp));
    DeleteFreeBlock(bp);
    
    // split 가능한 경우
    if((csize-asize) >= (16*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        InsertFreeBlock(bp, csize-asize);    
    }
    
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if(size == 0) return NULL;

    if(size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    // free list 탐색해서 적절한 block 이 있는 경우
    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);

    if((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;

    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{   
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size,0));
    PUT(FTRP(ptr), PACK(size,0));
    coalesce(ptr);
}


void *mm_realloc(void *bp, size_t size)
{   
    if(bp == NULL) return mm_malloc(size);
    
    if(size == 0){
        mm_free(bp);
        return NULL;
    }

    size_t csize = GET_SIZE(HDRP(bp));
    size_t asize;
    
    
    if(size <= DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if(asize <= csize){
        return bp;
    }

    // 새롭게 할당돼야 하는 공간이 더 큰 경우
    else{
        // next block 이 free block 인 경우, malloc 하지 않고 합쳐서 호출
        void* next_bp = NEXT_BLKP(bp);
        size_t next_alloc = GET_ALLOC(HDRP(next_bp));
        size_t nextsize = GET_SIZE(HDRP(next_bp));

        if(!next_alloc && csize + nextsize >= asize){
            DeleteFreeBlock(next_bp);
            PUT(HDRP(bp), PACK(csize+nextsize, 1));
            PUT(FTRP(bp), PACK(csize+nextsize, 1));
            return bp;
        }
        
        void* newptr = mm_malloc(size);
        if(newptr == NULL) return NULL;

        memcpy(newptr, bp, csize-2*WSIZE);
        mm_free(bp);
        return newptr;
    }
}
