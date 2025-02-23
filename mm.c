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
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8                                         //읽는 단위

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)       //size = 8의 배수로 정렬


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
////////////////////////////////////////////////////
/*Basic constants and macros*/
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)  //shift 연산

#define MAX(x,y) ((x)>(y)?(x):(y))

/*Pack a size and allocated bit into a word*/
#define PACK(size, alloc) ((size) | (alloc))        //size || alloc =>header data

/*Read and write a word at address p*/
#define GET(p) (*(unsigned int *)(p))               //read
#define PUT(p, val) (*(unsigned int *)(p) = (val))  //write

/*Read the size and allocated fileds from address p*/
#define GET_SIZE(p) (GET(p) & ~0x7)                 //size는 8의 배수 => 아마 header에 있을것으로 추정
#define GET_ALLOC(p) (GET(p) & 0x1)                 //할당: 1, 비어있음: 0 

/*Given block ptr bp, compute address of its header and footer*/
#define HDRP(bp) ((char*)(bp) - WSIZE)                        //Header Pointer
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)   //Footer Pointer

/*Given block ptr bp, compute address of next and previous blocks*/
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) //다음 블록 data 시작
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) //이전 블록 data 시작
///////////////////////////////////////////////////////////////////////////

static void* extend_heap(size_t words);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void* coalesce(void* bp);
/* 
 * mm_init - initialize the malloc package.
 */
static char* heap_listp; // = prolog block 속 블록 시작점(초기화는 if문에서 할 예정)
int mm_init(void)
{
    //heap할당 시 실패하면 (void*)-1 반환 =>초기화 실패
    //메모리 할당 성공 시 => 새로 확장된 힙 공간의 시작 주소 반환
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1) return -1; 
    PUT(heap_listp, 0);                             //padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    //Prologe Header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    //Prologe Footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        //Epilogue Header
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    if (extend_heap(4)==NULL) return -1;
    return 0;
}

static void* extend_heap(size_t words){
    char* bp;
    size_t size;

    size = (words%2) ? (words+1)*WSIZE : words*WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
/*
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}*/
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char* bp;

    if(size == 0) return NULL;

    if (size<=DSIZE) asize = 2*DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

//first fit: 맞는 블록 찾을 시 return
static void* find_fit(size_t asize){
    void* bp;
    for(bp = heap_listp; GET_SIZE(HDRP(bp))>0; bp = NEXT_BLKP(bp)){
        if ((!GET_ALLOC(HDRP(bp))) && (asize <= GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL;
}

//블록 분할: 남은 크기가 기본 크기보다 크거나 같으면 잘라쓴다!
static void place(void* bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    //분할
    if ((csize-asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    //분할x
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

//case1~4 경계 태그 사용한 블록 합체!!
static void* coalesce(void* bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    //prev, next블록이 모두 찼을 때, 블록 합체 실패
    if (prev_alloc && next_alloc){
        return bp;
    }
    //prev만 할당 상태일 때, next랑만 합체
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); //footer에서 가져오면 안되나??????????
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        
    }
    else{
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
 /*
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
   
    //원래 메모리가 없다? => 새 메모리 할당
    if (ptr == NULL){
        return mm_malloc(size);
    }
    
    //(size = 0) : free
    if (size == 0){
        mm_free(ptr);
        return NULL;
    }

    //size <= 현 size : 그냥 써라
    if(size <= GET_SIZE(HDRP(ptr))){
        return ptr;
    }

    newptr = mm_malloc(size);
    //할당 실패 시
    if (newptr == NULL)
      return NULL;
    //기존 데이터 새 데이터로 복사
    
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}*/



/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);
    if (newptr == NULL) {
        return NULL;
    }

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr))-DSIZE;  //copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize) {
        copySize = size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}