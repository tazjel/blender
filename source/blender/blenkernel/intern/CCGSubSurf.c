/* $Id$ */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "CCGSubSurf.h"

#include "MEM_guardedalloc.h"
#include "BLO_sys_types.h" // for intptr_t support

#ifdef _MSC_VER
#define CCG_INLINE __inline
#else
#define CCG_INLINE inline
#endif

/* copied from BKE_utildefines.h ugh */
#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) x
#endif

/* used for normalize_v3 in BLI_math_vector
 * float.h's FLT_EPSILON causes trouble with subsurf normals - campbell */
#define EPSILON (1.0e-35f)

/***/

typedef unsigned char	byte;

/***/

static int kHashSizes[] = {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 268435459
};

typedef struct _EHEntry EHEntry;
struct _EHEntry {
	EHEntry *next;
	void *key;
};
typedef struct _EHash {
	EHEntry **buckets;
	int numEntries, curSize, curSizeIdx;

	CCGAllocatorIFC allocatorIFC;
	CCGAllocatorHDL allocator;
} EHash;

#define EHASH_alloc(eh, nb)			((eh)->allocatorIFC.alloc((eh)->allocator, nb))
#define EHASH_free(eh, ptr)			((eh)->allocatorIFC.free((eh)->allocator, ptr))

#define EHASH_hash(eh, item)	(((uintptr_t) (item))%((unsigned int) (eh)->curSize))

static EHash *_ehash_new(int estimatedNumEntries, CCGAllocatorIFC *allocatorIFC, CCGAllocatorHDL allocator) {
	EHash *eh = allocatorIFC->alloc(allocator, sizeof(*eh));
	eh->allocatorIFC = *allocatorIFC;
	eh->allocator = allocator;
	eh->numEntries = 0;
	eh->curSizeIdx = 0;
	while (kHashSizes[eh->curSizeIdx]<estimatedNumEntries)
		eh->curSizeIdx++;
	eh->curSize = kHashSizes[eh->curSizeIdx];
	eh->buckets = EHASH_alloc(eh, eh->curSize*sizeof(*eh->buckets));
	memset(eh->buckets, 0, eh->curSize*sizeof(*eh->buckets));

	return eh;
}
typedef void (*EHEntryFreeFP)(EHEntry *, void *);
static void _ehash_free(EHash *eh, EHEntryFreeFP freeEntry, void *userData) {
	int numBuckets = eh->curSize;

	while (numBuckets--) {
		EHEntry *entry = eh->buckets[numBuckets];

		while (entry) {
			EHEntry *next = entry->next;

			freeEntry(entry, userData);

			entry = next;
		}
	}

	EHASH_free(eh, eh->buckets);
	EHASH_free(eh, eh);
}

static void _ehash_insert(EHash *eh, EHEntry *entry) {
	int numBuckets = eh->curSize;
	int hash = EHASH_hash(eh, entry->key);
	entry->next = eh->buckets[hash];
	eh->buckets[hash] = entry;
	eh->numEntries++;

	if (eh->numEntries > (numBuckets*3)) {
		EHEntry **oldBuckets = eh->buckets;
		eh->curSize = kHashSizes[++eh->curSizeIdx];
		
		eh->buckets = EHASH_alloc(eh, eh->curSize*sizeof(*eh->buckets));
		memset(eh->buckets, 0, eh->curSize*sizeof(*eh->buckets));

		while (numBuckets--) {
			for (entry = oldBuckets[numBuckets]; entry;) {
				EHEntry *next = entry->next;
				
				hash = EHASH_hash(eh, entry->key);
				entry->next = eh->buckets[hash];
				eh->buckets[hash] = entry;
				
				entry = next;
			}
		}

		EHASH_free(eh, oldBuckets);
	}
}

static void *_ehash_lookupWithPrev(EHash *eh, void *key, void ***prevp_r) {
	int hash = EHASH_hash(eh, key);
	void **prevp = (void**) &eh->buckets[hash];
	EHEntry *entry;
	
	for (; (entry = *prevp); prevp = (void**) &entry->next) {
		if (entry->key==key) {
			*prevp_r = (void**) prevp;
			return entry;
		}
	}
	
	return NULL;
}

static void *_ehash_lookup(EHash *eh, void *key) {
	int hash = EHASH_hash(eh, key);
	EHEntry *entry;
	
	for (entry = eh->buckets[hash]; entry; entry = entry->next)
		if (entry->key==key)
			break;
	
	return entry;
}

/**/

typedef struct _EHashIterator {
	EHash *eh;
	int curBucket;
	EHEntry *curEntry;
} EHashIterator;

static EHashIterator *_ehashIterator_new(EHash *eh) {
	EHashIterator *ehi = EHASH_alloc(eh, sizeof(*ehi));
	ehi->eh = eh;
	ehi->curEntry = NULL;
	ehi->curBucket = -1;
	while (!ehi->curEntry) {
		ehi->curBucket++;
		if (ehi->curBucket==ehi->eh->curSize)
			break;
		ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
	}
	return ehi;
}
static void _ehashIterator_free(EHashIterator *ehi) {
	EHASH_free(ehi->eh, ehi);
}

static void *_ehashIterator_getCurrent(EHashIterator *ehi) {
	return ehi->curEntry;
}

static void _ehashIterator_next(EHashIterator *ehi) {
	if (ehi->curEntry) {
		ehi->curEntry = ehi->curEntry->next;
		while (!ehi->curEntry) {
			ehi->curBucket++;
			if (ehi->curBucket==ehi->eh->curSize)
				break;
			ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
		}
	}
}
static int _ehashIterator_isStopped(EHashIterator *ehi) {
	return !ehi->curEntry;
}

/***/

static void *_stdAllocator_alloc(CCGAllocatorHDL UNUSED(a), int numBytes) {
	return malloc(numBytes);
}
static void *_stdAllocator_realloc(CCGAllocatorHDL UNUSED(a), void *ptr, int newSize, int UNUSED(oldSize)) {
	return realloc(ptr, newSize);
}
static void _stdAllocator_free(CCGAllocatorHDL UNUSED(a), void *ptr) {
	free(ptr);
}

static CCGAllocatorIFC *_getStandardAllocatorIFC(void) {
	static CCGAllocatorIFC ifc;

	ifc.alloc = _stdAllocator_alloc;
	ifc.realloc = _stdAllocator_realloc;
	ifc.free = _stdAllocator_free;
	ifc.release = NULL;

	return &ifc;
}

/***/

static int VertDataEqual(float *a, float *b) {
	return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];
}

#if 0
#define VertDataZero(av)				{ float *_a = (float*) av; _a[0] = _a[1] = _a[2] = 0.0f; }
#define VertDataCopy(av, bv)			{ float *_a = (float*) av, *_b = (float*) bv; _a[0] =_b[0]; _a[1] =_b[1]; _a[2] =_b[2]; }
#define VertDataAdd(av, bv)				{ float *_a = (float*) av, *_b = (float*) bv; _a[0]+=_b[0]; _a[1]+=_b[1]; _a[2]+=_b[2]; }
#define VertDataSub(av, bv)				{ float *_a = (float*) av, *_b = (float*) bv; _a[0]-=_b[0]; _a[1]-=_b[1]; _a[2]-=_b[2]; }
#define VertDataMulN(av, n)				{ float *_a = (float*) av; _a[0]*=n; _a[1]*=n; _a[2]*=n; }
#define VertDataAvg4(tv, av, bv, cv, dv) \
	{ \
		float *_t = (float*) tv, *_a = (float*) av, *_b = (float*) bv, *_c = (float*) cv, *_d = (float*) dv; \
		_t[0] = (_a[0]+_b[0]+_c[0]+_d[0])*.25; \
		_t[1] = (_a[1]+_b[1]+_c[1]+_d[1])*.25; \
		_t[2] = (_a[2]+_b[2]+_c[2]+_d[2])*.25; \
	}
#else

CCG_INLINE void VertDataZero(void *a, const int n)
{
	int i;
	for(i = 0; i < n; ++i)
		((float*)a)[i] = 0;
}

CCG_INLINE void VertDataCopy(void *a, void *b, int n)
{
	int i;
	for(i = 0; i < n; ++i)
		((float*)a)[i] = ((float*)b)[i];
}

CCG_INLINE void VertDataAdd(void *a, void *b, int n)
{
	int i;
	for(i = 0; i < n; ++i)
		((float*)a)[i] += ((float*)b)[i];
}

CCG_INLINE void VertDataSub(void *a, void *b, int n)
{
	int i;
	for(i = 0; i < n; ++i)
		((float*)a)[i] -= ((float*)b)[i];
}

CCG_INLINE void VertDataMulN(void *a, float b, int n)
{
	int i;
	for(i = 0; i < n; ++i)
		((float*)a)[i] *= b;
}

CCG_INLINE void VertDataAvg4(void *t, void *a, void *b, void *c, void *d, int n)
{
	int i;
	for(i = 0; i < n; ++i)
		((float*)t)[i] = (((float*)a)[i] + ((float*)b)[i] + ((float*)c)[i] + ((float*)d)[i]) * 0.25f;
}
#endif

#define NormZero(av)					{ float *_a = (float*) av; _a[0] = _a[1] = _a[2] = 0.0f; }
#define NormCopy(av, bv)				{ float *_a = (float*) av, *_b = (float*) bv; _a[0] =_b[0]; _a[1] =_b[1]; _a[2] =_b[2]; }
#define NormAdd(av, bv)					{ float *_a = (float*) av, *_b = (float*) bv; _a[0]+=_b[0]; _a[1]+=_b[1]; _a[2]+=_b[2]; }


static int _edge_isBoundary(CCGEdge *e);

/***/

enum {
	Vert_eEffected=		(1<<0),
	Vert_eChanged=		(1<<1),
	Vert_eSeam=			(1<<2),
} VertFlags;
enum {
	Edge_eEffected=		(1<<0),
} CCGEdgeFlags;
enum {
	Face_eEffected=		(1<<0),
} FaceFlags;

struct _CCGVert {
	CCGVert		*next;	/* EHData.next */
	CCGVertHDL	vHDL;	/* EHData.key */

	short numEdges, numFaces, flags, pad;

	CCGEdge **edges;
	CCGFace **faces;
//	byte *levelData;
//	byte *userData;
};
#define VERT_getLevelData(v)		((byte*) &(v)[1])

struct _CCGEdge {
	CCGEdge		*next;	/* EHData.next */
	CCGEdgeHDL	eHDL;	/* EHData.key */

	short numFaces, flags;
	float crease;

	CCGVert *v0,*v1;
	CCGFace **faces;

//	byte *levelData;
//	byte *userData;
};
#define EDGE_getLevelData(e)		((byte*) &(e)[1])

struct _CCGFace {
	CCGFace		*next;	/* EHData.next */
	CCGFaceHDL	fHDL;	/* EHData.key */

	short numVerts, flags, pad1, pad2;

//	CCGVert **verts;
//	CCGEdge **edges;
//	byte *centerData;
//	byte **gridData;
//	byte *userData;
};
#define FACE_getVerts(f)		((CCGVert**) &(f)[1])
#define FACE_getEdges(f)		((CCGEdge**) &(FACE_getVerts(f)[(f)->numVerts]))
#define FACE_getCenterData(f)	((byte*) &(FACE_getEdges(f)[(f)->numVerts]))

typedef enum {
	eSyncState_None = 0,
	eSyncState_Vert,
	eSyncState_Edge,
	eSyncState_Face,
	eSyncState_Partial,
} SyncState;

struct _CCGSubSurf {
	EHash *vMap;	/* map of CCGVertHDL -> Vert */
	EHash *eMap;	/* map of CCGEdgeHDL -> Edge */
	EHash *fMap;	/* map of CCGFaceHDL -> Face */

	CCGMeshIFC meshIFC;
	
	CCGAllocatorIFC allocatorIFC;
	CCGAllocatorHDL allocator;

	int subdivLevels;
	int numGrids;
	int allowEdgeCreation;
	float defaultCreaseValue;
	void *defaultEdgeUserData;

	void *q, *r;
		
	// data for calc vert normals
	int calcVertNormals;
	int normalDataOffset;

		// data for age'ing (to debug sync)
	int currentAge;
	int useAgeCounts;
	int vertUserAgeOffset;
	int edgeUserAgeOffset;
	int faceUserAgeOffset;

		// data used during syncing
	SyncState syncState;

	EHash *oldVMap, *oldEMap, *oldFMap;
	int lenTempArrays;
	CCGVert **tempVerts;
	CCGEdge **tempEdges;
};

#define CCGSUBSURF_alloc(ss, nb)			((ss)->allocatorIFC.alloc((ss)->allocator, nb))
#define CCGSUBSURF_realloc(ss, ptr, nb, ob)	((ss)->allocatorIFC.realloc((ss)->allocator, ptr, nb, ob))
#define CCGSUBSURF_free(ss, ptr)			((ss)->allocatorIFC.free((ss)->allocator, ptr))

/***/

static CCGVert *_vert_new(CCGVertHDL vHDL, CCGSubSurf *ss) {
	CCGVert *v = CCGSUBSURF_alloc(ss, sizeof(CCGVert) + ss->meshIFC.vertDataSize * (ss->subdivLevels+1) + ss->meshIFC.vertUserSize);
	byte *userData;

	v->vHDL = vHDL;
	v->edges = NULL;
	v->faces = NULL;
	v->numEdges = v->numFaces = 0;
	v->flags = 0;

	userData = ccgSubSurf_getVertUserData(ss, v);
	memset(userData, 0, ss->meshIFC.vertUserSize);
	if (ss->useAgeCounts) *((int*) &userData[ss->vertUserAgeOffset]) = ss->currentAge;

	return v;
}
static void _vert_remEdge(CCGVert *v, CCGEdge *e) {
	int i;
	for (i=0; i<v->numEdges; i++) {
		if (v->edges[i]==e) {
			v->edges[i] = v->edges[--v->numEdges];
			break;
		}
	}
}
static void _vert_remFace(CCGVert *v, CCGFace *f) {
	int i;
	for (i=0; i<v->numFaces; i++) {
		if (v->faces[i]==f) {
			v->faces[i] = v->faces[--v->numFaces];
			break;
		}
	}
}
static void _vert_addEdge(CCGVert *v, CCGEdge *e, CCGSubSurf *ss) {
	v->edges = CCGSUBSURF_realloc(ss, v->edges, (v->numEdges+1)*sizeof(*v->edges), v->numEdges*sizeof(*v->edges));
	v->edges[v->numEdges++] = e;
}
static void _vert_addFace(CCGVert *v, CCGFace *f, CCGSubSurf *ss) {
	v->faces = CCGSUBSURF_realloc(ss, v->faces, (v->numFaces+1)*sizeof(*v->faces), v->numFaces*sizeof(*v->faces));
	v->faces[v->numFaces++] = f;
}
static CCGEdge *_vert_findEdgeTo(CCGVert *v, CCGVert *vQ) {
	int i;
	for (i=0; i<v->numEdges; i++) {
		CCGEdge *e = v->edges[v->numEdges-1-i]; // XXX, note reverse
		if (	(e->v0==v && e->v1==vQ) ||
				(e->v1==v && e->v0==vQ))
			return e;
	}
	return 0;
}
static int _vert_isBoundary(CCGVert *v) {
	int i;
	for (i=0; i<v->numEdges; i++)
		if (_edge_isBoundary(v->edges[i]))
			return 1;
	return 0;
}

static void *_vert_getCo(CCGVert *v, int lvl, int dataSize) {
	return &VERT_getLevelData(v)[lvl*dataSize];
}
static float *_vert_getNo(CCGVert *v, int lvl, int dataSize, int normalDataOffset) {
	return (float*) &VERT_getLevelData(v)[lvl*dataSize + normalDataOffset];
}

static void _vert_free(CCGVert *v, CCGSubSurf *ss) {
	CCGSUBSURF_free(ss, v->edges);
	CCGSUBSURF_free(ss, v->faces);
	CCGSUBSURF_free(ss, v);
}

static int VERT_seam(CCGVert *v) {
	return ((v->flags & Vert_eSeam) != 0);
}

/***/

static CCGEdge *_edge_new(CCGEdgeHDL eHDL, CCGVert *v0, CCGVert *v1, float crease, CCGSubSurf *ss) {
	CCGEdge *e = CCGSUBSURF_alloc(ss, sizeof(CCGEdge) + ss->meshIFC.vertDataSize *((ss->subdivLevels+1) + (1<<(ss->subdivLevels+1))-1) + ss->meshIFC.edgeUserSize);
	byte *userData;

	e->eHDL = eHDL;
	e->v0 = v0;
	e->v1 = v1;
	e->crease = crease;
	e->faces = NULL;
	e->numFaces = 0;
	e->flags = 0;
	_vert_addEdge(v0, e, ss);
	_vert_addEdge(v1, e, ss);

	userData = ccgSubSurf_getEdgeUserData(ss, e);
	memset(userData, 0, ss->meshIFC.edgeUserSize);
	if (ss->useAgeCounts) *((int*) &userData[ss->edgeUserAgeOffset]) = ss->currentAge;

	return e;
}
static void _edge_remFace(CCGEdge *e, CCGFace *f) {
	int i;
	for (i=0; i<e->numFaces; i++) {
		if (e->faces[i]==f) {
			e->faces[i] = e->faces[--e->numFaces];
			break;
		}
	}
}
static void _edge_addFace(CCGEdge *e, CCGFace *f, CCGSubSurf *ss) {
	e->faces = CCGSUBSURF_realloc(ss, e->faces, (e->numFaces+1)*sizeof(*e->faces), e->numFaces*sizeof(*e->faces));
	e->faces[e->numFaces++] = f;
}
static int _edge_isBoundary(CCGEdge *e) {
	return e->numFaces<2;
}

static CCGVert *_edge_getOtherVert(CCGEdge *e, CCGVert *vQ) {
	if (vQ==e->v0) {
		return e->v1;
	} else {
		return e->v0;
	}
}

static void *_edge_getCo(CCGEdge *e, int lvl, int x, int dataSize) {
	int levelBase = lvl + (1<<lvl) - 1;
	return &EDGE_getLevelData(e)[dataSize*(levelBase + x)];
}
static float *_edge_getNo(CCGEdge *e, int lvl, int x, int dataSize, int normalDataOffset) {
	int levelBase = lvl + (1<<lvl) - 1;
	return (float*) &EDGE_getLevelData(e)[dataSize*(levelBase + x) + normalDataOffset];
}
static void *_edge_getCoVert(CCGEdge *e, CCGVert *v, int lvl, int x, int dataSize) {
	int levelBase = lvl + (1<<lvl) - 1;
	if (v==e->v0) {
		return &EDGE_getLevelData(e)[dataSize*(levelBase + x)];
	} else {
		return &EDGE_getLevelData(e)[dataSize*(levelBase + (1<<lvl) - x)];		
	}
}

static void _edge_free(CCGEdge *e, CCGSubSurf *ss) {
	CCGSUBSURF_free(ss, e->faces);
	CCGSUBSURF_free(ss, e);
}
static void _edge_unlinkMarkAndFree(CCGEdge *e, CCGSubSurf *ss) {
	_vert_remEdge(e->v0, e);
	_vert_remEdge(e->v1, e);
	e->v0->flags |= Vert_eEffected;
	e->v1->flags |= Vert_eEffected;
	_edge_free(e, ss);
}

static float EDGE_getSharpness(CCGEdge *e, int lvl) {
	if (!lvl)
		return e->crease;
	else if (!e->crease)
		return 0.0;
	else if (e->crease - lvl < 0.0)
		return 0.0;
	else
		return e->crease - lvl;
}

static CCGFace *_face_new(CCGFaceHDL fHDL, CCGVert **verts, CCGEdge **edges, int numVerts, CCGSubSurf *ss) {
	int maxGridSize = 1 + (1<<(ss->subdivLevels-1));
	CCGFace *f = CCGSUBSURF_alloc(ss, sizeof(CCGFace) + sizeof(CCGVert*)*numVerts + sizeof(CCGEdge*)*numVerts + ss->meshIFC.vertDataSize *(1 + numVerts*maxGridSize + numVerts*maxGridSize*maxGridSize) + ss->meshIFC.faceUserSize);
	byte *userData;
	int i;

	f->numVerts = numVerts;
	f->fHDL = fHDL;
	f->flags = 0;

	for (i=0; i<numVerts; i++) {
		FACE_getVerts(f)[i] = verts[i];
		FACE_getEdges(f)[i] = edges[i];
		_vert_addFace(verts[i], f, ss);
		_edge_addFace(edges[i], f, ss);
	}

	userData = ccgSubSurf_getFaceUserData(ss, f);
	memset(userData, 0, ss->meshIFC.faceUserSize);
	if (ss->useAgeCounts) *((int*) &userData[ss->faceUserAgeOffset]) = ss->currentAge;

	return f;
}

static CCG_INLINE void *_face_getIECo(CCGFace *f, int lvl, int S, int x, int levels, int dataSize) {
	int maxGridSize = 1 + (1<<(levels-1));
	int spacing = 1<<(levels-lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize*(1 + S*(maxGridSize + maxGridSize*maxGridSize));
	return &gridBase[dataSize*x*spacing];
}
static CCG_INLINE void *_face_getIENo(CCGFace *f, int lvl, int S, int x, int levels, int dataSize, int normalDataOffset) {
	int maxGridSize = 1 + (1<<(levels-1));
	int spacing = 1<<(levels-lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize*(1 + S*(maxGridSize + maxGridSize*maxGridSize));
	return &gridBase[dataSize*x*spacing + normalDataOffset];
}
static CCG_INLINE void *_face_getIFCo(CCGFace *f, int lvl, int S, int x, int y, int levels, int dataSize) {
	int maxGridSize = 1 + (1<<(levels-1));
	int spacing = 1<<(levels-lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize*(1 + S*(maxGridSize + maxGridSize*maxGridSize));
	return &gridBase[dataSize*(maxGridSize + (y*maxGridSize + x)*spacing)];
}
static CCG_INLINE float *_face_getIFNo(CCGFace *f, int lvl, int S, int x, int y, int levels, int dataSize, int normalDataOffset) {
	int maxGridSize = 1 + (1<<(levels-1));
	int spacing = 1<<(levels-lvl);
	byte *gridBase = FACE_getCenterData(f) + dataSize*(1 + S*(maxGridSize + maxGridSize*maxGridSize));
	return (float*) &gridBase[dataSize*(maxGridSize + (y*maxGridSize + x)*spacing) + normalDataOffset];
}
static int _face_getVertIndex(CCGFace *f, CCGVert *v) {
	int i;
	for (i=0; i<f->numVerts; i++)
		if (FACE_getVerts(f)[i]==v)
			return i;
	return -1;
}
static CCG_INLINE void *_face_getIFCoEdge(CCGFace *f, CCGEdge *e, int lvl, int eX, int eY, int levels, int dataSize) {
	int maxGridSize = 1 + (1<<(levels-1));
	int spacing = 1<<(levels-lvl);
	int S, x, y, cx, cy;

	for (S=0; S<f->numVerts; S++)
		if (FACE_getEdges(f)[S]==e)
			break;

	eX = eX*spacing;
	eY = eY*spacing;
	if (e->v0!=FACE_getVerts(f)[S]) {
		eX = (maxGridSize*2 - 1)-1 - eX;
	}
	y = maxGridSize - 1 - eX;
	x = maxGridSize - 1 - eY;
	if (x<0) {
		S = (S+f->numVerts-1)%f->numVerts;
		cx = y;
		cy = -x;
	} else if (y<0) {
		S = (S+1)%f->numVerts;
		cx = -y;
		cy = x;
	} else {
		cx = x;
		cy = y;
	}
	return _face_getIFCo(f, levels, S, cx, cy, levels, dataSize);
}
static float *_face_getIFNoEdge(CCGFace *f, CCGEdge *e, int lvl, int eX, int eY, int levels, int dataSize, int normalDataOffset) {
	return (float*) ((byte*) _face_getIFCoEdge(f, e, lvl, eX, eY, levels, dataSize) + normalDataOffset);
}
void _face_calcIFNo(CCGFace *f, int lvl, int S, int x, int y, float *no, int levels, int dataSize) {
	float *a = _face_getIFCo(f, lvl, S, x+0, y+0, levels, dataSize);
	float *b = _face_getIFCo(f, lvl, S, x+1, y+0, levels, dataSize);
	float *c = _face_getIFCo(f, lvl, S, x+1, y+1, levels, dataSize);
	float *d = _face_getIFCo(f, lvl, S, x+0, y+1, levels, dataSize);
	float a_cX = c[0]-a[0], a_cY = c[1]-a[1], a_cZ = c[2]-a[2];
	float b_dX = d[0]-b[0], b_dY = d[1]-b[1], b_dZ = d[2]-b[2];
	float length;

	no[0] = b_dY*a_cZ - b_dZ*a_cY;
	no[1] = b_dZ*a_cX - b_dX*a_cZ;
	no[2] = b_dX*a_cY - b_dY*a_cX;

	length = sqrt(no[0]*no[0] + no[1]*no[1] + no[2]*no[2]);

	if (length>EPSILON) {
		float invLength = 1.f/length;

		no[0] *= invLength;
		no[1] *= invLength;
		no[2] *= invLength;
	} else {
		NormZero(no);
	}
}

static void _face_free(CCGFace *f, CCGSubSurf *ss) {
	CCGSUBSURF_free(ss, f);
}
static void _face_unlinkMarkAndFree(CCGFace *f, CCGSubSurf *ss) {
	int j;
	for (j=0; j<f->numVerts; j++) {
		_vert_remFace(FACE_getVerts(f)[j], f);
		_edge_remFace(FACE_getEdges(f)[j], f);
		FACE_getVerts(f)[j]->flags |= Vert_eEffected;
	}
	_face_free(f, ss);
}

/***/

CCGSubSurf *ccgSubSurf_new(CCGMeshIFC *ifc, int subdivLevels, CCGAllocatorIFC *allocatorIFC, CCGAllocatorHDL allocator) {
	if (!allocatorIFC) {
		allocatorIFC = _getStandardAllocatorIFC();
		allocator = NULL;
	}

	if (subdivLevels<1) {
		return NULL;
	} else {
		CCGSubSurf *ss = allocatorIFC->alloc(allocator, sizeof(*ss));

		ss->allocatorIFC = *allocatorIFC;
		ss->allocator = allocator;

		ss->vMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->eMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->fMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);

		ss->meshIFC = *ifc;
		
		ss->subdivLevels = subdivLevels;
		ss->numGrids = 0;
		ss->allowEdgeCreation = 0;
		ss->defaultCreaseValue = 0;
		ss->defaultEdgeUserData = NULL;

		ss->useAgeCounts = 0;
		ss->vertUserAgeOffset = ss->edgeUserAgeOffset = ss->faceUserAgeOffset = 0;

		ss->calcVertNormals = 0;
		ss->normalDataOffset = 0;

		ss->q = CCGSUBSURF_alloc(ss, ss->meshIFC.vertDataSize);
		ss->r = CCGSUBSURF_alloc(ss, ss->meshIFC.vertDataSize);

		ss->currentAge = 0;

		ss->syncState = eSyncState_None;

		ss->oldVMap = ss->oldEMap = ss->oldFMap = NULL;
		ss->lenTempArrays = 0;
		ss->tempVerts = NULL;
		ss->tempEdges = NULL;	

		return ss;
	}
}

void ccgSubSurf_free(CCGSubSurf *ss) {
	CCGAllocatorIFC allocatorIFC = ss->allocatorIFC;
	CCGAllocatorHDL allocator = ss->allocator;

	if (ss->syncState) {
		_ehash_free(ss->oldFMap, (EHEntryFreeFP) _face_free, ss);
		_ehash_free(ss->oldEMap, (EHEntryFreeFP) _edge_free, ss);
		_ehash_free(ss->oldVMap, (EHEntryFreeFP) _vert_free, ss);

		MEM_freeN(ss->tempVerts);
		MEM_freeN(ss->tempEdges);
	}

	CCGSUBSURF_free(ss, ss->r);
	CCGSUBSURF_free(ss, ss->q);
	if (ss->defaultEdgeUserData) CCGSUBSURF_free(ss, ss->defaultEdgeUserData);

	_ehash_free(ss->fMap, (EHEntryFreeFP) _face_free, ss);
	_ehash_free(ss->eMap, (EHEntryFreeFP) _edge_free, ss);
	_ehash_free(ss->vMap, (EHEntryFreeFP) _vert_free, ss);

	/* free gridkey */
	if(ss->meshIFC.gridkey.color_names)
		MEM_freeN(ss->meshIFC.gridkey.color_names);
	if(ss->meshIFC.gridkey.mask_names)
		MEM_freeN(ss->meshIFC.gridkey.mask_names);

	ss->meshIFC.gridkey.color_names = NULL;
	ss->meshIFC.gridkey.mask_names = NULL;

	CCGSUBSURF_free(ss, ss);

	if (allocatorIFC.release) {
		allocatorIFC.release(allocator);
	}
}

CCGError ccgSubSurf_setAllowEdgeCreation(CCGSubSurf *ss, int allowEdgeCreation, float defaultCreaseValue, void *defaultUserData) {
	if (ss->defaultEdgeUserData) {
		CCGSUBSURF_free(ss, ss->defaultEdgeUserData);
	}

	ss->allowEdgeCreation = !!allowEdgeCreation;
	ss->defaultCreaseValue = defaultCreaseValue;
	ss->defaultEdgeUserData = CCGSUBSURF_alloc(ss, ss->meshIFC.edgeUserSize);

	if (defaultUserData) {
		memcpy(ss->defaultEdgeUserData, defaultUserData, ss->meshIFC.edgeUserSize);
	} else {
		memset(ss->defaultEdgeUserData, 0, ss->meshIFC.edgeUserSize);
	}

	return eCCGError_None;
}
void ccgSubSurf_getAllowEdgeCreation(CCGSubSurf *ss, int *allowEdgeCreation_r, float *defaultCreaseValue_r, void *defaultUserData_r) {
	if (allowEdgeCreation_r) *allowEdgeCreation_r = ss->allowEdgeCreation;
	if (ss->allowEdgeCreation) {
		if (defaultCreaseValue_r) *defaultCreaseValue_r = ss->defaultCreaseValue;
		if (defaultUserData_r) memcpy(defaultUserData_r, ss->defaultEdgeUserData, ss->meshIFC.edgeUserSize);
	}
}

CCGError ccgSubSurf_setSubdivisionLevels(CCGSubSurf *ss, int subdivisionLevels) {
	if (subdivisionLevels<=0) {
		return eCCGError_InvalidValue;
	} else if (subdivisionLevels!=ss->subdivLevels) {
		ss->numGrids = 0;
		ss->subdivLevels = subdivisionLevels;
		_ehash_free(ss->vMap, (EHEntryFreeFP) _vert_free, ss);
		_ehash_free(ss->eMap, (EHEntryFreeFP) _edge_free, ss);
		_ehash_free(ss->fMap, (EHEntryFreeFP) _face_free, ss);
		ss->vMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->eMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
		ss->fMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
	}

	return eCCGError_None;
}

void ccgSubSurf_getUseAgeCounts(CCGSubSurf *ss, int *useAgeCounts_r, int *vertUserOffset_r, int *edgeUserOffset_r, int *faceUserOffset_r)
{
	*useAgeCounts_r = ss->useAgeCounts;

	if (vertUserOffset_r) *vertUserOffset_r = ss->vertUserAgeOffset;
	if (edgeUserOffset_r) *edgeUserOffset_r = ss->edgeUserAgeOffset;
	if (faceUserOffset_r) *faceUserOffset_r = ss->faceUserAgeOffset;
}

CCGError ccgSubSurf_setUseAgeCounts(CCGSubSurf *ss, int useAgeCounts, int vertUserOffset, int edgeUserOffset, int faceUserOffset) {
	if (useAgeCounts) {
		if (	(vertUserOffset+4>ss->meshIFC.vertUserSize) ||
				(edgeUserOffset+4>ss->meshIFC.edgeUserSize) ||
				(faceUserOffset+4>ss->meshIFC.faceUserSize)) {
			return eCCGError_InvalidValue;
		}  else {
			ss->useAgeCounts = 1;
			ss->vertUserAgeOffset = vertUserOffset;
			ss->edgeUserAgeOffset = edgeUserOffset;
			ss->faceUserAgeOffset = faceUserOffset;
		}
	} else {
		ss->useAgeCounts = 0;
		ss->vertUserAgeOffset = ss->edgeUserAgeOffset = ss->faceUserAgeOffset = 0;
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_setCalcVertexNormals(CCGSubSurf *ss, int useVertNormals, int normalDataOffset) {
	if (useVertNormals) {
		if (normalDataOffset<0 || normalDataOffset+12>ss->meshIFC.vertDataSize) {
			return eCCGError_InvalidValue;
		} else {
			ss->calcVertNormals = 1;
			ss->normalDataOffset = normalDataOffset;
		}
	} else {
		ss->calcVertNormals = 0;
		ss->normalDataOffset = 0;
	}

	return eCCGError_None;
}

/***/

CCGError ccgSubSurf_initFullSync(CCGSubSurf *ss) {
	if (ss->syncState!=eSyncState_None) {
		return eCCGError_InvalidSyncState;
	}

	ss->currentAge++;

	ss->oldVMap = ss->vMap; 
	ss->oldEMap = ss->eMap; 
	ss->oldFMap = ss->fMap;

	ss->vMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
	ss->eMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);
	ss->fMap = _ehash_new(0, &ss->allocatorIFC, ss->allocator);

	ss->numGrids = 0;

	ss->lenTempArrays = 12;
	ss->tempVerts = MEM_mallocN(sizeof(*ss->tempVerts)*ss->lenTempArrays, "CCGSubsurf tempVerts");
	ss->tempEdges = MEM_mallocN(sizeof(*ss->tempEdges)*ss->lenTempArrays, "CCGSubsurf tempEdges");

	ss->syncState = eSyncState_Vert;

	return eCCGError_None;
}

CCGError ccgSubSurf_initPartialSync(CCGSubSurf *ss) {
	if (ss->syncState!=eSyncState_None) {
		return eCCGError_InvalidSyncState;
	}

	ss->currentAge++;

	ss->syncState = eSyncState_Partial;

	return eCCGError_None;
}

CCGError ccgSubSurf_syncVertDel(CCGSubSurf *ss, CCGVertHDL vHDL) {
	if (ss->syncState!=eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	} else {
		void **prevp;
		CCGVert *v = _ehash_lookupWithPrev(ss->vMap, vHDL, &prevp);

		if (!v || v->numFaces || v->numEdges) {
			return eCCGError_InvalidValue;
		} else {
			*prevp = v->next;
			_vert_free(v, ss);
		}
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_syncEdgeDel(CCGSubSurf *ss, CCGEdgeHDL eHDL) {
	if (ss->syncState!=eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	} else {
		void **prevp;
		CCGEdge *e = _ehash_lookupWithPrev(ss->eMap, eHDL, &prevp);

		if (!e || e->numFaces) {
			return eCCGError_InvalidValue;
		} else {
			*prevp = e->next;
			_edge_unlinkMarkAndFree(e, ss);
		}
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_syncFaceDel(CCGSubSurf *ss, CCGFaceHDL fHDL) {
	if (ss->syncState!=eSyncState_Partial) {
		return eCCGError_InvalidSyncState;
	} else {
		void **prevp;
		CCGFace *f = _ehash_lookupWithPrev(ss->fMap, fHDL, &prevp);

		if (!f) {
			return eCCGError_InvalidValue;
		} else {
			*prevp = f->next;
			_face_unlinkMarkAndFree(f, ss);
		}
	}

	return eCCGError_None;
}

CCGError ccgSubSurf_syncVert(CCGSubSurf *ss, CCGVertHDL vHDL, void *vertData, int seam, CCGVert **v_r) {
	void **prevp;
	CCGVert *v = NULL;
	short seamflag = (seam)? Vert_eSeam: 0;
	
	if (ss->syncState==eSyncState_Partial) {
		v = _ehash_lookupWithPrev(ss->vMap, vHDL, &prevp);
		if (!v) {
			v = _vert_new(vHDL, ss);
			VertDataCopy(_vert_getCo(v,0,ss->meshIFC.vertDataSize), vertData, ss->meshIFC.finterpCount);
			_ehash_insert(ss->vMap, (EHEntry*) v);
			v->flags = Vert_eEffected|seamflag;
		} else if (!VertDataEqual(vertData, _vert_getCo(v, 0, ss->meshIFC.vertDataSize)) || ((v->flags & Vert_eSeam) != seamflag)) {
			int i, j;

			VertDataCopy(_vert_getCo(v,0,ss->meshIFC.vertDataSize), vertData, ss->meshIFC.finterpCount);
			v->flags = Vert_eEffected|seamflag;

			for (i=0; i<v->numEdges; i++) {
				CCGEdge *e = v->edges[i];
				e->v0->flags |= Vert_eEffected;
				e->v1->flags |= Vert_eEffected;
			}
			for (i=0; i<v->numFaces; i++) {
				CCGFace *f = v->faces[i];
				for (j=0; j<f->numVerts; j++) {
					FACE_getVerts(f)[j]->flags |= Vert_eEffected;
				}
			}
		}
	} else {
		if (ss->syncState!=eSyncState_Vert) { 
			return eCCGError_InvalidSyncState;
		}

		v = _ehash_lookupWithPrev(ss->oldVMap, vHDL, &prevp);
		if (!v) {
			v = _vert_new(vHDL, ss);
			VertDataCopy(_vert_getCo(v,0,ss->meshIFC.vertDataSize), vertData, ss->meshIFC.finterpCount);
			_ehash_insert(ss->vMap, (EHEntry*) v);
			v->flags = Vert_eEffected|seamflag;
		} else if (!VertDataEqual(vertData, _vert_getCo(v, 0, ss->meshIFC.vertDataSize)) || ((v->flags & Vert_eSeam) != seamflag)) {
			*prevp = v->next;
			_ehash_insert(ss->vMap, (EHEntry*) v);
			VertDataCopy(_vert_getCo(v,0,ss->meshIFC.vertDataSize), vertData, ss->meshIFC.finterpCount);
			v->flags = Vert_eEffected|Vert_eChanged|seamflag;
		} else {
			*prevp = v->next;
			_ehash_insert(ss->vMap, (EHEntry*) v);
			v->flags = 0;
		}
	}

	if (v_r) *v_r = v;
	return eCCGError_None;
}

CCGError ccgSubSurf_syncEdge(CCGSubSurf *ss, CCGEdgeHDL eHDL, CCGVertHDL e_vHDL0, CCGVertHDL e_vHDL1, float crease, CCGEdge **e_r) {
	void **prevp;
	CCGEdge *e = NULL, *eNew;

	if (ss->syncState==eSyncState_Partial) {
		e = _ehash_lookupWithPrev(ss->eMap, eHDL, &prevp);
		if (!e || e->v0->vHDL!=e_vHDL0 || e->v1->vHDL!=e_vHDL1 || crease!=e->crease) {
			CCGVert *v0 = _ehash_lookup(ss->vMap, e_vHDL0);
			CCGVert *v1 = _ehash_lookup(ss->vMap, e_vHDL1);

			eNew = _edge_new(eHDL, v0, v1, crease, ss);

			if (e) {
				*prevp = eNew;
				eNew->next = e->next;

				_edge_unlinkMarkAndFree(e, ss);
			} else {
				_ehash_insert(ss->eMap, (EHEntry*) eNew);
			}

			eNew->v0->flags |= Vert_eEffected;
			eNew->v1->flags |= Vert_eEffected;
		}
	} else {
		if (ss->syncState==eSyncState_Vert) {
			ss->syncState = eSyncState_Edge;
		} else if (ss->syncState!=eSyncState_Edge) {
			return eCCGError_InvalidSyncState;
		}

		e = _ehash_lookupWithPrev(ss->oldEMap, eHDL, &prevp);
		if (!e || e->v0->vHDL!=e_vHDL0 || e->v1->vHDL!=e_vHDL1|| e->crease!=crease) {
			CCGVert *v0 = _ehash_lookup(ss->vMap, e_vHDL0);
			CCGVert *v1 = _ehash_lookup(ss->vMap, e_vHDL1);
			e = _edge_new(eHDL, v0, v1, crease, ss);
			_ehash_insert(ss->eMap, (EHEntry*) e);
			e->v0->flags |= Vert_eEffected;
			e->v1->flags |= Vert_eEffected;
		} else {
			*prevp = e->next;
			_ehash_insert(ss->eMap, (EHEntry*) e);
			e->flags = 0;
			if ((e->v0->flags|e->v1->flags)&Vert_eChanged) {
				e->v0->flags |= Vert_eEffected;
				e->v1->flags |= Vert_eEffected;
			}
		}
	}

	if (e_r) *e_r = e;
	return eCCGError_None;
}

CCGError ccgSubSurf_syncFace(CCGSubSurf *ss, CCGFaceHDL fHDL, int numVerts, CCGVertHDL *vHDLs, CCGFace **f_r) {
	void **prevp;
	CCGFace *f = NULL, *fNew;
	int j, k, topologyChanged = 0;

	if (numVerts>ss->lenTempArrays) {
		ss->lenTempArrays = (numVerts<ss->lenTempArrays*2)?ss->lenTempArrays*2:numVerts;
		ss->tempVerts = MEM_reallocN(ss->tempVerts, sizeof(*ss->tempVerts)*ss->lenTempArrays);
		ss->tempEdges = MEM_reallocN(ss->tempEdges, sizeof(*ss->tempEdges)*ss->lenTempArrays);
	}

	if (ss->syncState==eSyncState_Partial) {
		f = _ehash_lookupWithPrev(ss->fMap, fHDL, &prevp);

		for (k=0; k<numVerts; k++) {
			ss->tempVerts[k] = _ehash_lookup(ss->vMap, vHDLs[k]);
		}
		for (k=0; k<numVerts; k++) {
			ss->tempEdges[k] = _vert_findEdgeTo(ss->tempVerts[k], ss->tempVerts[(k+1)%numVerts]);
		}

		if (f) {
			if (	f->numVerts!=numVerts ||
					memcmp(FACE_getVerts(f), ss->tempVerts, sizeof(*ss->tempVerts)*numVerts) ||
					memcmp(FACE_getEdges(f), ss->tempEdges, sizeof(*ss->tempEdges)*numVerts))
				topologyChanged = 1;
		}

		if (!f || topologyChanged) {
			fNew = _face_new(fHDL, ss->tempVerts, ss->tempEdges, numVerts, ss);

			if (f) {
				ss->numGrids += numVerts - f->numVerts;

				*prevp = fNew;
				fNew->next = f->next;

				_face_unlinkMarkAndFree(f, ss);
			} else {
				ss->numGrids += numVerts;
				_ehash_insert(ss->fMap, (EHEntry*) fNew);
			}

			for (k=0; k<numVerts; k++)
				FACE_getVerts(fNew)[k]->flags |= Vert_eEffected;
		}
	} else {
		if (ss->syncState==eSyncState_Vert || ss->syncState==eSyncState_Edge) {
			ss->syncState = eSyncState_Face;
		} else if (ss->syncState!=eSyncState_Face) {
			return eCCGError_InvalidSyncState;
		}

		f = _ehash_lookupWithPrev(ss->oldFMap, fHDL, &prevp);

		for (k=0; k<numVerts; k++) {
			ss->tempVerts[k] = _ehash_lookup(ss->vMap, vHDLs[k]);

			if (!ss->tempVerts[k])
				return eCCGError_InvalidValue;
		}
		for (k=0; k<numVerts; k++) {
			ss->tempEdges[k] = _vert_findEdgeTo(ss->tempVerts[k], ss->tempVerts[(k+1)%numVerts]);

			if (!ss->tempEdges[k]) {
				if (ss->allowEdgeCreation) {
					CCGEdge *e = ss->tempEdges[k] = _edge_new((CCGEdgeHDL) -1, ss->tempVerts[k], ss->tempVerts[(k+1)%numVerts], ss->defaultCreaseValue, ss);
					_ehash_insert(ss->eMap, (EHEntry*) e);
					e->v0->flags |= Vert_eEffected;
					e->v1->flags |= Vert_eEffected;
					if (ss->meshIFC.edgeUserSize) {
						memcpy(ccgSubSurf_getEdgeUserData(ss, e), ss->defaultEdgeUserData, ss->meshIFC.edgeUserSize);
					}
				} else {
					return eCCGError_InvalidValue;
				}
			}
		}

		if (f) {
			if (	f->numVerts!=numVerts ||
					memcmp(FACE_getVerts(f), ss->tempVerts, sizeof(*ss->tempVerts)*numVerts) ||
					memcmp(FACE_getEdges(f), ss->tempEdges, sizeof(*ss->tempEdges)*numVerts))
				topologyChanged = 1;
		}

		if (!f || topologyChanged) {
			f = _face_new(fHDL, ss->tempVerts, ss->tempEdges, numVerts, ss);
			_ehash_insert(ss->fMap, (EHEntry*) f);
			ss->numGrids += numVerts;

			for (k=0; k<numVerts; k++)
				FACE_getVerts(f)[k]->flags |= Vert_eEffected;
		} else {
			*prevp = f->next;
			_ehash_insert(ss->fMap, (EHEntry*) f);
			f->flags = 0;
			ss->numGrids += f->numVerts;

			for (j=0; j<f->numVerts; j++) {
				if (FACE_getVerts(f)[j]->flags&Vert_eChanged) {
					for (k=0; k<f->numVerts; k++)
						FACE_getVerts(f)[k]->flags |= Vert_eEffected;
					break;
				}
			}
		}
	}

	if (f_r) *f_r = f;
	return eCCGError_None;
}

static void ccgSubSurf__sync(CCGSubSurf *ss);
CCGError ccgSubSurf_processSync(CCGSubSurf *ss) {
	if (ss->syncState==eSyncState_Partial) {
		ss->syncState = eSyncState_None;

		ccgSubSurf__sync(ss);
	} else if (ss->syncState) {
		_ehash_free(ss->oldFMap, (EHEntryFreeFP) _face_unlinkMarkAndFree, ss);
		_ehash_free(ss->oldEMap, (EHEntryFreeFP) _edge_unlinkMarkAndFree, ss);
		_ehash_free(ss->oldVMap, (EHEntryFreeFP) _vert_free, ss);
		MEM_freeN(ss->tempEdges);
		MEM_freeN(ss->tempVerts);

		ss->lenTempArrays = 0;

		ss->oldFMap = ss->oldEMap = ss->oldVMap = NULL;
		ss->tempVerts = NULL;
		ss->tempEdges = NULL;

		ss->syncState = eSyncState_None;

		ccgSubSurf__sync(ss);
	} else {
		return eCCGError_InvalidSyncState;
	}

	return eCCGError_None;
}

#define VERT_getNo(e, lvl)					_vert_getNo(e, lvl, vertDataSize, normalDataOffset)
#define EDGE_getNo(e, lvl, x)				_edge_getNo(e, lvl, x, vertDataSize, normalDataOffset)
#define FACE_getIFNo(f, lvl, S, x, y)		_face_getIFNo(f, lvl, S, x, y, subdivLevels, vertDataSize, normalDataOffset)
#define FACE_calcIFNo(f, lvl, S, x, y, no)	_face_calcIFNo(f, lvl, S, x, y, no, subdivLevels, vertDataSize)
#define FACE_getIENo(f, lvl, S, x)			_face_getIENo(f, lvl, S, x, subdivLevels, vertDataSize, normalDataOffset)
static void ccgSubSurf__calcVertNormals(CCGSubSurf *ss,
	CCGVert **effectedV, CCGEdge **effectedE, CCGFace **effectedF,
	int numEffectedV, int numEffectedE, int numEffectedF) {
	int i,ptrIdx;
	int subdivLevels = ss->subdivLevels;
	int lvl = ss->subdivLevels;
	int edgeSize = 1 + (1<<lvl);
	int gridSize = 1 + (1<<(lvl-1));
	int normalDataOffset = ss->normalDataOffset;
	int vertDataSize = ss->meshIFC.vertDataSize;

	#pragma omp parallel for private(ptrIdx) if(numEffectedF*edgeSize*edgeSize*4 >= CCG_OMP_LIMIT)
	for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
		CCGFace *f = (CCGFace*) effectedF[ptrIdx];
		int S, x, y;
		float no[3];

		for (S=0; S<f->numVerts; S++) {
			for (y=0; y<gridSize-1; y++)
				for (x=0; x<gridSize-1; x++)
					NormZero(FACE_getIFNo(f, lvl, S, x, y));

			if (FACE_getEdges(f)[(S-1+f->numVerts)%f->numVerts]->flags&Edge_eEffected)
				for (x=0; x<gridSize-1; x++)
					NormZero(FACE_getIFNo(f, lvl, S, x, gridSize-1));
			if (FACE_getEdges(f)[S]->flags&Edge_eEffected)
				for (y=0; y<gridSize-1; y++)
					NormZero(FACE_getIFNo(f, lvl, S, gridSize-1, y));
			if (FACE_getVerts(f)[S]->flags&Vert_eEffected)
				NormZero(FACE_getIFNo(f, lvl, S, gridSize-1, gridSize-1));
		}

		for (S=0; S<f->numVerts; S++) {
			int yLimit = !(FACE_getEdges(f)[(S-1+f->numVerts)%f->numVerts]->flags&Edge_eEffected);
			int xLimit = !(FACE_getEdges(f)[S]->flags&Edge_eEffected);
			int yLimitNext = xLimit;
			int xLimitPrev = yLimit;
			
			for (y=0; y<gridSize - 1; y++) {
				for (x=0; x<gridSize - 1; x++) {
					int xPlusOk = (!xLimit || x<gridSize-2);
					int yPlusOk = (!yLimit || y<gridSize-2);

					FACE_calcIFNo(f, lvl, S, x, y, no);

					NormAdd(FACE_getIFNo(f, lvl, S, x+0, y+0), no);
					if (xPlusOk)
						NormAdd(FACE_getIFNo(f, lvl, S, x+1, y+0), no);
					if (yPlusOk)
						NormAdd(FACE_getIFNo(f, lvl, S, x+0, y+1), no);
					if (xPlusOk && yPlusOk) {
						if (x<gridSize-2 || y<gridSize-2 || FACE_getVerts(f)[S]->flags&Vert_eEffected) {
							NormAdd(FACE_getIFNo(f, lvl, S, x+1, y+1), no);
						}
					}

					if (x==0 && y==0) {
						int K;

						if (!yLimitNext || 1<gridSize-1)
							NormAdd(FACE_getIFNo(f, lvl, (S+1)%f->numVerts, 0, 1), no);
						if (!xLimitPrev || 1<gridSize-1)
							NormAdd(FACE_getIFNo(f, lvl, (S-1+f->numVerts)%f->numVerts, 1, 0), no);

						for (K=0; K<f->numVerts; K++) {
							if (K!=S) {
								NormAdd(FACE_getIFNo(f, lvl, K, 0, 0), no);
							}
						}
					} else if (y==0) {
						NormAdd(FACE_getIFNo(f, lvl, (S+1)%f->numVerts, 0, x), no);
						if (!yLimitNext || x<gridSize-2)
							NormAdd(FACE_getIFNo(f, lvl, (S+1)%f->numVerts, 0, x+1), no);
					} else if (x==0) {
						NormAdd(FACE_getIFNo(f, lvl, (S-1+f->numVerts)%f->numVerts, y, 0), no);
						if (!xLimitPrev || y<gridSize-2)
							NormAdd(FACE_getIFNo(f, lvl, (S-1+f->numVerts)%f->numVerts, y+1, 0), no);
					}
				}
			}
		}
	}
		// XXX can I reduce the number of normalisations here?
	for (ptrIdx=0; ptrIdx<numEffectedV; ptrIdx++) {
		CCGVert *v = (CCGVert*) effectedV[ptrIdx];
		float length, *no = _vert_getNo(v, lvl, vertDataSize, normalDataOffset);

		NormZero(no);

		for (i=0; i<v->numFaces; i++) {
			CCGFace *f = v->faces[i];
			NormAdd(no, FACE_getIFNo(f, lvl, _face_getVertIndex(f,v), gridSize-1, gridSize-1));
		}

		length = sqrt(no[0]*no[0] + no[1]*no[1] + no[2]*no[2]);

		if (length>EPSILON) {
			float invLength = 1.0f/length;
			no[0] *= invLength;
			no[1] *= invLength;
			no[2] *= invLength;
		} else {
			NormZero(no);
		}

		for (i=0; i<v->numFaces; i++) {
			CCGFace *f = v->faces[i];
			NormCopy(FACE_getIFNo(f, lvl, _face_getVertIndex(f,v), gridSize-1, gridSize-1), no);
		}
	}
	for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
		CCGEdge *e = (CCGEdge*) effectedE[ptrIdx];

		if (e->numFaces) {
			CCGFace *fLast = e->faces[e->numFaces-1];
			int x;

			for (i=0; i<e->numFaces-1; i++) {
				CCGFace *f = e->faces[i];

				for (x=1; x<edgeSize-1; x++) {
					NormAdd(_face_getIFNoEdge(fLast, e, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset),
							_face_getIFNoEdge(f, e, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
				}
			}

			for (i=0; i<e->numFaces-1; i++) {
				CCGFace *f = e->faces[i];

				for (x=1; x<edgeSize-1; x++) {
					NormCopy(_face_getIFNoEdge(f, e, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset),
							_face_getIFNoEdge(fLast, e, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
				}
			}
		}
	}

	#pragma omp parallel for private(ptrIdx) if(numEffectedF*edgeSize*edgeSize*4 >= CCG_OMP_LIMIT)
	for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
		CCGFace *f = (CCGFace*) effectedF[ptrIdx];
		int S, x, y;

		for (S=0; S<f->numVerts; S++) {
			NormCopy(FACE_getIFNo(f, lvl, (S+1)%f->numVerts, 0, gridSize-1),
					 FACE_getIFNo(f, lvl, S, gridSize-1, 0));
		}

		for (S=0; S<f->numVerts; S++) {
			for (y=0; y<gridSize; y++) {
				for (x=0; x<gridSize; x++) {
					float *no = FACE_getIFNo(f, lvl, S, x, y);
					float length = sqrt(no[0]*no[0] + no[1]*no[1] + no[2]*no[2]);

					if (length>EPSILON) {
						float invLength = 1.0f/length;
						no[0] *= invLength;
						no[1] *= invLength;
						no[2] *= invLength;
					} else {
						NormZero(no);
					}
				}
			}

			VertDataCopy((float*)((byte*)FACE_getCenterData(f) + normalDataOffset),
				     FACE_getIFNo(f, lvl, S, 0, 0), 3);

			for (x=1; x<gridSize-1; x++)
				NormCopy(FACE_getIENo(f, lvl, S, x),
					FACE_getIFNo(f, lvl, S, x, 0));
		}
	}

	for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
		CCGEdge *e = (CCGEdge*) effectedE[ptrIdx];

		if (e->numFaces) {
			CCGFace *f = e->faces[0];
			int x;

			for (x=0; x<edgeSize; x++)
				NormCopy(EDGE_getNo(e, lvl, x),
					_face_getIFNoEdge(f, e, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
		}
	}
}
#undef FACE_getIFNo

#define VERT_getCo(v, lvl)				_vert_getCo(v, lvl, vertDataSize)
#define EDGE_getCo(e, lvl, x)			_edge_getCo(e, lvl, x, vertDataSize)
#define FACE_getIECo(f, lvl, S, x)		_face_getIECo(f, lvl, S, x, subdivLevels, vertDataSize)
#define FACE_getIFCo(f, lvl, S, x, y)	_face_getIFCo(f, lvl, S, x, y, subdivLevels, vertDataSize)
static void ccgSubSurf__calcSubdivLevel(CCGSubSurf *ss,
	CCGVert **effectedV, CCGEdge **effectedE, CCGFace **effectedF,
	int numEffectedV, int numEffectedE, int numEffectedF, int curLvl) {
	int subdivLevels = ss->subdivLevels;
	int edgeSize = 1 + (1<<curLvl);
	int gridSize = 1 + (1<<(curLvl-1));
	int nextLvl = curLvl+1;
	int ptrIdx, cornerIdx, i;
	int vertDataSize = ss->meshIFC.vertDataSize;
	int finterpCount = ss->meshIFC.finterpCount;
	void *q = ss->q, *r = ss->r;

	#pragma omp parallel for private(ptrIdx) if(numEffectedF*edgeSize*edgeSize*4 >= CCG_OMP_LIMIT)
	for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
		CCGFace *f = (CCGFace*) effectedF[ptrIdx];
		int S, x, y;

			/* interior face midpoints
			 *  o old interior face points
			 */
		for (S=0; S<f->numVerts; S++) {
			for (y=0; y<gridSize-1; y++) {
				for (x=0; x<gridSize-1; x++) {
					int fx = 1 + 2*x;
					int fy = 1 + 2*y;
					void *co0 = FACE_getIFCo(f, curLvl, S, x+0, y+0);
					void *co1 = FACE_getIFCo(f, curLvl, S, x+1, y+0);
					void *co2 = FACE_getIFCo(f, curLvl, S, x+1, y+1);
					void *co3 = FACE_getIFCo(f, curLvl, S, x+0, y+1);
					void *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

					VertDataAvg4(co, co0, co1, co2, co3, finterpCount);
				}
			}
		}

			/* interior edge midpoints
			 *  o old interior edge points
			 *  o new interior face midpoints
			 */
		for (S=0; S<f->numVerts; S++) {
			for (x=0; x<gridSize-1; x++) {
				int fx = x*2 + 1;
				void *co0 = FACE_getIECo(f, curLvl, S, x+0);
				void *co1 = FACE_getIECo(f, curLvl, S, x+1);
				void *co2 = FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 1, fx);
				void *co3 = FACE_getIFCo(f, nextLvl, S, fx, 1);
				void *co = FACE_getIECo(f, nextLvl, S, fx);
				
				VertDataAvg4(co, co0, co1, co2, co3, finterpCount);
			}

					/* interior face interior edge midpoints
					 *  o old interior face points
					 *  o new interior face midpoints
					 */

				/* vertical */
			for (x=1; x<gridSize-1; x++) {
				for (y=0; y<gridSize-1; y++) {
					int fx = x*2;
					int fy = y*2+1;
					void *co0 = FACE_getIFCo(f, curLvl, S, x, y+0);
					void *co1 = FACE_getIFCo(f, curLvl, S, x, y+1);
					void *co2 = FACE_getIFCo(f, nextLvl, S, fx-1, fy);
					void *co3 = FACE_getIFCo(f, nextLvl, S, fx+1, fy);
					void *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

					VertDataAvg4(co, co0, co1, co2, co3, finterpCount);
				}
			}

				/* horizontal */
			for (y=1; y<gridSize-1; y++) {
				for (x=0; x<gridSize-1; x++) {
					int fx = x*2+1;
					int fy = y*2;
					void *co0 = FACE_getIFCo(f, curLvl, S, x+0, y);
					void *co1 = FACE_getIFCo(f, curLvl, S, x+1, y);
					void *co2 = FACE_getIFCo(f, nextLvl, S, fx, fy-1);
					void *co3 = FACE_getIFCo(f, nextLvl, S, fx, fy+1);
					void *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

					VertDataAvg4(co, co0, co1, co2, co3, finterpCount);
				}
			}
		}
	}

		/* exterior edge midpoints
		 *  o old exterior edge points
		 *  o new interior face midpoints
		 */
	for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
		CCGEdge *e = (CCGEdge*) effectedE[ptrIdx];
		float sharpness = EDGE_getSharpness(e, curLvl);
		int x, j;

		if (_edge_isBoundary(e) || sharpness>1.0) {
			for (x=0; x<edgeSize-1; x++) {
				int fx = x*2 + 1;
				void *co0 = EDGE_getCo(e, curLvl, x+0);
				void *co1 = EDGE_getCo(e, curLvl, x+1);
				void *co = EDGE_getCo(e, nextLvl, fx);

				VertDataCopy(co, co0, finterpCount);
				VertDataAdd(co, co1, finterpCount);
				VertDataMulN(co, 0.5, finterpCount);
			}
		} else {
			for (x=0; x<edgeSize-1; x++) {
				int fx = x*2 + 1;
				void *co0 = EDGE_getCo(e, curLvl, x+0);
				void *co1 = EDGE_getCo(e, curLvl, x+1);
				void *co = EDGE_getCo(e, nextLvl, fx);
				int numFaces = 0;

				VertDataCopy(q, co0, finterpCount);
				VertDataAdd(q, co1, finterpCount);

				for (j=0; j<e->numFaces; j++) {
					CCGFace *f = e->faces[j];
					VertDataAdd(q, _face_getIFCoEdge(f, e, nextLvl, fx, 1, subdivLevels, vertDataSize), finterpCount);
					numFaces++;
				}

				VertDataMulN(q, 1.0f/(2.0f+numFaces), finterpCount);

				VertDataCopy(r, co0, finterpCount);
				VertDataAdd(r, co1, finterpCount);
				VertDataMulN(r, 0.5, finterpCount);

				VertDataCopy(co, q, finterpCount);
				VertDataSub(r, q, finterpCount);
				VertDataMulN(r, sharpness, finterpCount);
				VertDataAdd(co, r, finterpCount);
			}
		}
	}

		/* exterior vertex shift
		 *  o old vertex points (shifting)
		 *  o old exterior edge points
		 *  o new interior face midpoints
		 */
	for (ptrIdx=0; ptrIdx<numEffectedV; ptrIdx++) {
		CCGVert *v = (CCGVert*) effectedV[ptrIdx];
		void *co = VERT_getCo(v, curLvl);
		void *nCo = VERT_getCo(v, nextLvl);
		int sharpCount = 0, allSharp = 1;
		float avgSharpness = 0.0;
		int j, seam = VERT_seam(v), seamEdges = 0;

		for (j=0; j<v->numEdges; j++) {
			CCGEdge *e = v->edges[j];
			float sharpness = EDGE_getSharpness(e, curLvl);

			if (seam && _edge_isBoundary(e))
				seamEdges++;

			if (sharpness!=0.0f) {
				sharpCount++;
				avgSharpness += sharpness;
			} else {
				allSharp = 0;
			}
		}

		if(sharpCount) {
			avgSharpness /= sharpCount;
			if (avgSharpness>1.0) {
				avgSharpness = 1.0;
			}
		}

		if (seam && seamEdges < 2)
			seam = 0;

		if (!v->numEdges) {
			VertDataCopy(nCo, co, finterpCount);
		} else if (_vert_isBoundary(v)) {
			int numBoundary = 0;

			VertDataZero(r, finterpCount);
			for (j=0; j<v->numEdges; j++) {
				CCGEdge *e = v->edges[j];
				if (_edge_isBoundary(e)) {
					VertDataAdd(r, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), finterpCount);
					numBoundary++;
				}
			}

			VertDataCopy(nCo, co, finterpCount);
			VertDataMulN(nCo, 0.75, finterpCount);
			VertDataMulN(r, 0.25f/numBoundary, finterpCount);
			VertDataAdd(nCo, r, finterpCount);
		} else {
			int cornerIdx = (1 + (1<<(curLvl))) - 2;
			int numEdges = 0, numFaces = 0;

			VertDataZero(q, finterpCount);
			for (j=0; j<v->numFaces; j++) {
				CCGFace *f = v->faces[j];
				VertDataAdd(q, FACE_getIFCo(f, nextLvl, _face_getVertIndex(f,v), cornerIdx, cornerIdx), finterpCount);
				numFaces++;
			}
			VertDataMulN(q, 1.0f/numFaces, finterpCount);
			VertDataZero(r, finterpCount);
			for (j=0; j<v->numEdges; j++) {
				CCGEdge *e = v->edges[j];
				VertDataAdd(r, _edge_getCoVert(e, v, curLvl, 1,vertDataSize), finterpCount);
				numEdges++;
			}
			VertDataMulN(r, 1.0f/numEdges, finterpCount);

			VertDataCopy(nCo, co, finterpCount);
			VertDataMulN(nCo, numEdges-2.0f, finterpCount);
			VertDataAdd(nCo, q, finterpCount);
			VertDataAdd(nCo, r, finterpCount);
			VertDataMulN(nCo, 1.0f/numEdges, finterpCount);
		}

		if ((sharpCount>1 && v->numFaces) || seam) {
			VertDataZero(q, finterpCount);

			if (seam) {
				avgSharpness = 1.0f;
				sharpCount = seamEdges;
				allSharp = 1;
			}

			for (j=0; j<v->numEdges; j++) {
				CCGEdge *e = v->edges[j];
				float sharpness = EDGE_getSharpness(e, curLvl);

				if (seam) {
					if (_edge_isBoundary(e))
						VertDataAdd(q, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), finterpCount);
				} else if (sharpness != 0.0) {
					VertDataAdd(q, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), finterpCount);
				}
			}

			VertDataMulN(q, (float) 1/sharpCount, finterpCount);

			if (sharpCount!=2 || allSharp) {
					// q = q + (co-q)*avgSharpness
				VertDataCopy(r, co, finterpCount);
				VertDataSub(r, q, finterpCount);
				VertDataMulN(r, avgSharpness, finterpCount);
				VertDataAdd(q, r, finterpCount);
			}

				// r = co*.75 + q*.25
			VertDataCopy(r, co, finterpCount);
			VertDataMulN(r, .75, finterpCount);
			VertDataMulN(q, .25, finterpCount);
			VertDataAdd(r, q, finterpCount);

				// nCo = nCo  + (r-nCo)*avgSharpness
			VertDataSub(r, nCo, finterpCount);
			VertDataMulN(r, avgSharpness, finterpCount);
			VertDataAdd(nCo, r, finterpCount);
		}
	}

		/* exterior edge interior shift
		 *  o old exterior edge midpoints (shifting)
		 *  o old exterior edge midpoints
		 *  o new interior face midpoints
		 */
	for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
		CCGEdge *e = (CCGEdge*) effectedE[ptrIdx];
		float sharpness = EDGE_getSharpness(e, curLvl);
		int sharpCount = 0;
		float avgSharpness = 0.0;
		int x, j;

		if (sharpness!=0.0f) {
			sharpCount = 2;
			avgSharpness += sharpness;

			if (avgSharpness>1.0) {
				avgSharpness = 1.0;
			}
		} else {
			sharpCount = 0;
			avgSharpness = 0;
		}

		if (_edge_isBoundary(e) && (!e->numFaces || sharpCount<2)) {
			for (x=1; x<edgeSize-1; x++) {
				int fx = x*2;
				void *co = EDGE_getCo(e, curLvl, x);
				void *nCo = EDGE_getCo(e, nextLvl, fx);
				VertDataCopy(r, EDGE_getCo(e, curLvl, x-1), finterpCount);
				VertDataAdd(r, EDGE_getCo(e, curLvl, x+1), finterpCount);
				VertDataMulN(r, 0.5, finterpCount);
				VertDataCopy(nCo, co, finterpCount);
				VertDataMulN(nCo, 0.75, finterpCount);
				VertDataMulN(r, 0.25, finterpCount);
				VertDataAdd(nCo, r, finterpCount);
			}
		} else {
			for (x=1; x<edgeSize-1; x++) {
				int fx = x*2;
				void *co = EDGE_getCo(e, curLvl, x);
				void *nCo = EDGE_getCo(e, nextLvl, fx);
				int numFaces = 0;

				VertDataZero(q, finterpCount);
				VertDataZero(r, finterpCount);
				VertDataAdd(r, EDGE_getCo(e, curLvl, x-1), finterpCount);
				VertDataAdd(r, EDGE_getCo(e, curLvl, x+1), finterpCount);
				for (j=0; j<e->numFaces; j++) {
					CCGFace *f = e->faces[j];
					VertDataAdd(q, _face_getIFCoEdge(f, e, nextLvl, fx-1, 1, subdivLevels, vertDataSize), finterpCount);
					VertDataAdd(q, _face_getIFCoEdge(f, e, nextLvl, fx+1, 1, subdivLevels, vertDataSize), finterpCount);

					VertDataAdd(r, _face_getIFCoEdge(f, e, curLvl, x, 1, subdivLevels, vertDataSize), finterpCount);
					numFaces++;
				}
				VertDataMulN(q, 1.0/(numFaces*2.0f), finterpCount);
				VertDataMulN(r, 1.0/(2.0f + numFaces), finterpCount);

				VertDataCopy(nCo, co, finterpCount);
				VertDataMulN(nCo, (float) numFaces, finterpCount);
				VertDataAdd(nCo, q, finterpCount);
				VertDataAdd(nCo, r, finterpCount);
				VertDataMulN(nCo, 1.0f/(2+numFaces), finterpCount);

				if (sharpCount==2) {
					VertDataCopy(q, co, finterpCount);
					VertDataMulN(q, 6.0f, finterpCount);
					VertDataAdd(q, EDGE_getCo(e, curLvl, x-1), finterpCount);
					VertDataAdd(q, EDGE_getCo(e, curLvl, x+1), finterpCount);
					VertDataMulN(q, 1/8.0f, finterpCount);

					VertDataSub(q, nCo, finterpCount);
					VertDataMulN(q, avgSharpness, finterpCount);
					VertDataAdd(nCo, q, finterpCount);
				}
			}
		}
	}

	#pragma omp parallel private(ptrIdx) if(numEffectedF*edgeSize*edgeSize*4 >= CCG_OMP_LIMIT)
	{
		void *q, *r;

		#pragma omp critical
		{
			q = MEM_mallocN(ss->meshIFC.vertDataSize, "CCGSubsurf q");
			r = MEM_mallocN(ss->meshIFC.vertDataSize, "CCGSubsurf r");
		}

		#pragma omp for schedule(static)
		for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
			CCGFace *f = (CCGFace*) effectedF[ptrIdx];
			int S, x, y;

				/* interior center point shift
				 *  o old face center point (shifting)
				 *  o old interior edge points
				 *  o new interior face midpoints
				 */
			VertDataZero(q, finterpCount);
			for (S=0; S<f->numVerts; S++) {
				VertDataAdd(q, FACE_getIFCo(f, nextLvl, S, 1, 1), finterpCount);
			}
			VertDataMulN(q, 1.0f/f->numVerts, finterpCount);
			VertDataZero(r, finterpCount);
			for (S=0; S<f->numVerts; S++) {
				VertDataAdd(r, FACE_getIECo(f, curLvl, S, 1), finterpCount);
			}
			VertDataMulN(r, 1.0f/f->numVerts, finterpCount);

			VertDataMulN(FACE_getCenterData(f), f->numVerts-2.0f, finterpCount);
			VertDataAdd(FACE_getCenterData(f), q, finterpCount);
			VertDataAdd(FACE_getCenterData(f), r, finterpCount);
			VertDataMulN(FACE_getCenterData(f), 1.0f/f->numVerts, finterpCount);

			for (S=0; S<f->numVerts; S++) {
					/* interior face shift
					 *  o old interior face point (shifting)
					 *  o new interior edge midpoints
					 *  o new interior face midpoints
					 */
				for (x=1; x<gridSize-1; x++) {
					for (y=1; y<gridSize-1; y++) {
						int fx = x*2;
						int fy = y*2;
						void *co = FACE_getIFCo(f, curLvl, S, x, y);
						void *nCo = FACE_getIFCo(f, nextLvl, S, fx, fy);
						
						VertDataAvg4(q, FACE_getIFCo(f, nextLvl, S, fx-1, fy-1),
							     FACE_getIFCo(f, nextLvl, S, fx+1, fy-1),
							     FACE_getIFCo(f, nextLvl, S, fx+1, fy+1),
							     FACE_getIFCo(f, nextLvl, S, fx-1, fy+1), finterpCount);

						VertDataAvg4(r, FACE_getIFCo(f, nextLvl, S, fx-1, fy+0),
							     FACE_getIFCo(f, nextLvl, S, fx+1, fy+0),
							     FACE_getIFCo(f, nextLvl, S, fx+0, fy-1),
							     FACE_getIFCo(f, nextLvl, S, fx+0, fy+1), finterpCount);

						VertDataCopy(nCo, co, finterpCount);
						VertDataSub(nCo, q, finterpCount);
						VertDataMulN(nCo, 0.25f, finterpCount);
						VertDataAdd(nCo, r, finterpCount);
					}
				}

					/* interior edge interior shift
					 *  o old interior edge point (shifting)
					 *  o new interior edge midpoints
					 *  o new interior face midpoints
					 */
				for (x=1; x<gridSize-1; x++) {
					int fx = x*2;
					void *co = FACE_getIECo(f, curLvl, S, x);
					void *nCo = FACE_getIECo(f, nextLvl, S, fx);
					
					VertDataAvg4(q, FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 1, fx-1),
						     FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 1, fx+1),
						     FACE_getIFCo(f, nextLvl, S, fx+1, +1),
						     FACE_getIFCo(f, nextLvl, S, fx-1, +1), finterpCount);

					VertDataAvg4(r, FACE_getIECo(f, nextLvl, S, fx-1),
						     FACE_getIECo(f, nextLvl, S, fx+1),
						     FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 1, fx),
						     FACE_getIFCo(f, nextLvl, S, fx, 1), finterpCount);

					VertDataCopy(nCo, co, finterpCount);
					VertDataSub(nCo, q, finterpCount);
					VertDataMulN(nCo, 0.25f, finterpCount);
					VertDataAdd(nCo, r, finterpCount);
				}
			}
		}

		#pragma omp critical
		{
			MEM_freeN(q);
			MEM_freeN(r);
		}
	}

		/* copy down */
	edgeSize = 1 + (1<<(nextLvl));
	gridSize = 1 + (1<<((nextLvl)-1));
	cornerIdx = gridSize-1;

	#pragma omp parallel for private(i) if(numEffectedF*edgeSize*edgeSize*4 >= CCG_OMP_LIMIT)
	for (i=0; i<numEffectedE; i++) {
		CCGEdge *e = effectedE[i];
		VertDataCopy(EDGE_getCo(e, nextLvl, 0), VERT_getCo(e->v0, nextLvl), finterpCount);
		VertDataCopy(EDGE_getCo(e, nextLvl, edgeSize-1), VERT_getCo(e->v1, nextLvl), finterpCount);
	}

	#pragma omp parallel for private(i) if(numEffectedF*edgeSize*edgeSize*4 >= CCG_OMP_LIMIT)
	for (i=0; i<numEffectedF; i++) {
		CCGFace *f = effectedF[i];
		int S, x;

		for (S=0; S<f->numVerts; S++) {
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[(S+f->numVerts-1)%f->numVerts];

			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 0), FACE_getCenterData(f), finterpCount);
			VertDataCopy(FACE_getIECo(f, nextLvl, S, 0), FACE_getCenterData(f), finterpCount);
			VertDataCopy(FACE_getIFCo(f, nextLvl, S, cornerIdx, cornerIdx), VERT_getCo(FACE_getVerts(f)[S], nextLvl), finterpCount);
			VertDataCopy(FACE_getIECo(f, nextLvl, S, cornerIdx), EDGE_getCo(FACE_getEdges(f)[S], nextLvl, cornerIdx), finterpCount);
			for (x=1; x<gridSize-1; x++) {
				void *co = FACE_getIECo(f, nextLvl, S, x);
				VertDataCopy(FACE_getIFCo(f, nextLvl, S, x, 0), co, finterpCount);
				VertDataCopy(FACE_getIFCo(f, nextLvl, (S+1)%f->numVerts, 0, x), co, finterpCount);
			}
			for (x=0; x<gridSize-1; x++) {
				int eI = gridSize-1-x;
				VertDataCopy(FACE_getIFCo(f, nextLvl, S, cornerIdx, x), _edge_getCoVert(e, FACE_getVerts(f)[S], nextLvl, eI,vertDataSize), finterpCount);
				VertDataCopy(FACE_getIFCo(f, nextLvl, S, x, cornerIdx), _edge_getCoVert(prevE, FACE_getVerts(f)[S], nextLvl, eI,vertDataSize), finterpCount);
			}
		}
	}
}


static void ccgSubSurf__sync(CCGSubSurf *ss) {
	CCGVert **effectedV;
	CCGEdge **effectedE;
	CCGFace **effectedF;
	int numEffectedV, numEffectedE, numEffectedF;
	int subdivLevels = ss->subdivLevels;
	int vertDataSize = ss->meshIFC.vertDataSize;
	int finterpCount = ss->meshIFC.finterpCount;
	int i, j, ptrIdx, S;
	int curLvl, nextLvl;
	void *q = ss->q, *r = ss->r;

	effectedV = MEM_mallocN(sizeof(*effectedV)*ss->vMap->numEntries, "CCGSubsurf effectedV");
	effectedE = MEM_mallocN(sizeof(*effectedE)*ss->eMap->numEntries, "CCGSubsurf effectedE");
	effectedF = MEM_mallocN(sizeof(*effectedF)*ss->fMap->numEntries, "CCGSubsurf effectedF");
	numEffectedV = numEffectedE = numEffectedF = 0;
	for (i=0; i<ss->vMap->curSize; i++) {
		CCGVert *v = (CCGVert*) ss->vMap->buckets[i];
		for (; v; v = v->next) {
			if (v->flags&Vert_eEffected) {
				effectedV[numEffectedV++] = v;

				for (j=0; j<v->numEdges; j++) {
					CCGEdge *e = v->edges[j];
					if (!(e->flags&Edge_eEffected)) {
						effectedE[numEffectedE++] = e;
						e->flags |= Edge_eEffected;
					}
				}

				for (j=0; j<v->numFaces; j++) {
					CCGFace *f = v->faces[j];
					if (!(f->flags&Face_eEffected)) {
						effectedF[numEffectedF++] = f;
						f->flags |= Face_eEffected;
					}
				}
			}
		}
	}

	curLvl = 0;
	nextLvl = curLvl+1;

	for (ptrIdx=0; ptrIdx<numEffectedF; ptrIdx++) {
		CCGFace *f = effectedF[ptrIdx];
		void *co = FACE_getCenterData(f);
		VertDataZero(co, finterpCount);
		for (i=0; i<f->numVerts; i++) {
			VertDataAdd(co, VERT_getCo(FACE_getVerts(f)[i], curLvl), finterpCount);
		}
		VertDataMulN(co, 1.0f/f->numVerts, finterpCount);

		f->flags = 0;
	}
	for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
		CCGEdge *e = effectedE[ptrIdx];
		void *co = EDGE_getCo(e, nextLvl, 1);
		float sharpness = EDGE_getSharpness(e, curLvl);

		if (_edge_isBoundary(e) || sharpness>=1.0) {
			VertDataCopy(co, VERT_getCo(e->v0, curLvl), finterpCount);
			VertDataAdd(co, VERT_getCo(e->v1, curLvl), finterpCount);
			VertDataMulN(co, 0.5f, finterpCount);
		} else {
			int numFaces = 0;
			VertDataCopy(q, VERT_getCo(e->v0, curLvl), finterpCount);
			VertDataAdd(q, VERT_getCo(e->v1, curLvl), finterpCount);
			for (i=0; i<e->numFaces; i++) {
				CCGFace *f = e->faces[i];
				VertDataAdd(q, FACE_getCenterData(f), finterpCount);
				numFaces++;
			}
			VertDataMulN(q, 1.0f/(2.0f+numFaces), finterpCount);

			VertDataCopy(r, VERT_getCo(e->v0, curLvl), finterpCount);
			VertDataAdd(r, VERT_getCo(e->v1, curLvl), finterpCount);
			VertDataMulN(r, 0.5f, finterpCount);

			VertDataCopy(co, q, finterpCount);
			VertDataSub(r, q, finterpCount);
			VertDataMulN(r, sharpness, finterpCount);
			VertDataAdd(co, r, finterpCount);
		}

		// edge flags cleared later
	}
	for (ptrIdx=0; ptrIdx<numEffectedV; ptrIdx++) {
		CCGVert *v = effectedV[ptrIdx];
		void *co = VERT_getCo(v, curLvl);
		void *nCo = VERT_getCo(v, nextLvl);
		int sharpCount = 0, allSharp = 1;
		float avgSharpness = 0.0;
		int seam = VERT_seam(v), seamEdges = 0;

		for (i=0; i<v->numEdges; i++) {
			CCGEdge *e = v->edges[i];
			float sharpness = EDGE_getSharpness(e, curLvl);

			if (seam && _edge_isBoundary(e))
				seamEdges++;

			if (sharpness!=0.0f) {
				sharpCount++;
				avgSharpness += sharpness;
			} else {
				allSharp = 0;
			}
		}

		if(sharpCount) {
			avgSharpness /= sharpCount;
			if (avgSharpness>1.0) {
				avgSharpness = 1.0;
			}
		}

		if (seam && seamEdges < 2)
			seam = 0;

		if (!v->numEdges) {
			VertDataCopy(nCo, co, finterpCount);
		} else if (_vert_isBoundary(v)) {
			int numBoundary = 0;

			VertDataZero(r, finterpCount);
			for (i=0; i<v->numEdges; i++) {
				CCGEdge *e = v->edges[i];
				if (_edge_isBoundary(e)) {
					VertDataAdd(r, VERT_getCo(_edge_getOtherVert(e, v), curLvl), finterpCount);
					numBoundary++;
				}
			}
			VertDataCopy(nCo, co, finterpCount);
			VertDataMulN(nCo, 0.75, finterpCount);
			VertDataMulN(r, 0.25f/numBoundary, finterpCount);
			VertDataAdd(nCo, r, finterpCount);
		} else {
			int numEdges = 0, numFaces = 0;

			VertDataZero(q, finterpCount);
			for (i=0; i<v->numFaces; i++) {
				CCGFace *f = v->faces[i];
				VertDataAdd(q, FACE_getCenterData(f), finterpCount);
				numFaces++;
			}
			VertDataMulN(q, 1.0f/numFaces, finterpCount);
			VertDataZero(r, finterpCount);
			for (i=0; i<v->numEdges; i++) {
				CCGEdge *e = v->edges[i];
				VertDataAdd(r, VERT_getCo(_edge_getOtherVert(e, v), curLvl), finterpCount);
				numEdges++;
			}
			VertDataMulN(r, 1.0f/numEdges, finterpCount);

			VertDataCopy(nCo, co, finterpCount);
			VertDataMulN(nCo, numEdges-2.0f, finterpCount);
			VertDataAdd(nCo, q, finterpCount);
			VertDataAdd(nCo, r, finterpCount);
			VertDataMulN(nCo, 1.0f/numEdges, finterpCount);
		}

		if (sharpCount>1 || seam) {
			VertDataZero(q, finterpCount);

			if (seam) {
				avgSharpness = 1.0f;
				sharpCount = seamEdges;
				allSharp = 1;
			}

			for (i=0; i<v->numEdges; i++) {
				CCGEdge *e = v->edges[i];
				float sharpness = EDGE_getSharpness(e, curLvl);

				if (seam) {
					if (_edge_isBoundary(e)) {
						CCGVert *oV = _edge_getOtherVert(e, v);
						VertDataAdd(q, VERT_getCo(oV, curLvl), finterpCount);
					}
				} else if (sharpness != 0.0) {
					CCGVert *oV = _edge_getOtherVert(e, v);
					VertDataAdd(q, VERT_getCo(oV, curLvl), finterpCount);
				}
			}

			VertDataMulN(q, (float) 1/sharpCount, finterpCount);

			if (sharpCount!=2 || allSharp) {
					// q = q + (co-q)*avgSharpness
				VertDataCopy(r, co, finterpCount);
				VertDataSub(r, q, finterpCount);
				VertDataMulN(r, avgSharpness, finterpCount);
				VertDataAdd(q, r, finterpCount);
			}

				// r = co*.75 + q*.25
			VertDataCopy(r, co, finterpCount);
			VertDataMulN(r, .75, finterpCount);
			VertDataMulN(q, .25, finterpCount);
			VertDataAdd(r, q, finterpCount);

				// nCo = nCo  + (r-nCo)*avgSharpness
			VertDataSub(r, nCo, finterpCount);
			VertDataMulN(r, avgSharpness, finterpCount);
			VertDataAdd(nCo, r, finterpCount);
		}

		// vert flags cleared later
	}

	if (ss->useAgeCounts) {
		for (i=0; i<numEffectedV; i++) {
			CCGVert *v = effectedV[i];
			byte *userData = ccgSubSurf_getVertUserData(ss, v);
			*((int*) &userData[ss->vertUserAgeOffset]) = ss->currentAge;
		}

		for (i=0; i<numEffectedE; i++) {
			CCGEdge *e = effectedE[i];
			byte *userData = ccgSubSurf_getEdgeUserData(ss, e);
			*((int*) &userData[ss->edgeUserAgeOffset]) = ss->currentAge;
		}

		for (i=0; i<numEffectedF; i++) {
			CCGFace *f = effectedF[i];
			byte *userData = ccgSubSurf_getFaceUserData(ss, f);
			*((int*) &userData[ss->faceUserAgeOffset]) = ss->currentAge;
		}
	}

	for (i=0; i<numEffectedE; i++) {
		CCGEdge *e = effectedE[i];
		VertDataCopy(EDGE_getCo(e, nextLvl, 0), VERT_getCo(e->v0, nextLvl), finterpCount);
		VertDataCopy(EDGE_getCo(e, nextLvl, 2), VERT_getCo(e->v1, nextLvl), finterpCount);
	}
	for (i=0; i<numEffectedF; i++) {
		CCGFace *f = effectedF[i];
		for (S=0; S<f->numVerts; S++) {
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[(S+f->numVerts-1)%f->numVerts];

			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 0), FACE_getCenterData(f), finterpCount);
			VertDataCopy(FACE_getIECo(f, nextLvl, S, 0), FACE_getCenterData(f), finterpCount);
			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 1, 1), VERT_getCo(FACE_getVerts(f)[S], nextLvl), finterpCount);
			VertDataCopy(FACE_getIECo(f, nextLvl, S, 1), EDGE_getCo(FACE_getEdges(f)[S], nextLvl, 1), finterpCount);

			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 1, 0), _edge_getCoVert(e, FACE_getVerts(f)[S], nextLvl, 1, vertDataSize), finterpCount);
			VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 1), _edge_getCoVert(prevE, FACE_getVerts(f)[S], nextLvl, 1, vertDataSize), finterpCount);
		}
	}

	for (curLvl=1; curLvl<subdivLevels; curLvl++) {
		ccgSubSurf__calcSubdivLevel(ss,
			effectedV, effectedE, effectedF,
			numEffectedV, numEffectedE, numEffectedF, curLvl);
	}

	if (ss->calcVertNormals)
		ccgSubSurf__calcVertNormals(ss,
			effectedV, effectedE, effectedF,
			numEffectedV, numEffectedE, numEffectedF);

	for (ptrIdx=0; ptrIdx<numEffectedV; ptrIdx++) {
		CCGVert *v = effectedV[ptrIdx];
		v->flags = 0;
	}
	for (ptrIdx=0; ptrIdx<numEffectedE; ptrIdx++) {
		CCGEdge *e = effectedE[ptrIdx];
		e->flags = 0;
	}

	MEM_freeN(effectedF);
	MEM_freeN(effectedE);
	MEM_freeN(effectedV);
}

static void ccgSubSurf__allFaces(CCGSubSurf *ss, CCGFace ***faces, int *numFaces, int *freeFaces)
{
	CCGFace **array;
	int i, num;

	if(!*faces) {
		array = MEM_mallocN(sizeof(*array)*ss->fMap->numEntries, "CCGSubsurf allFaces");
		num = 0;
		for (i=0; i<ss->fMap->curSize; i++) {
			CCGFace *f = (CCGFace*) ss->fMap->buckets[i];

			for (; f; f = f->next)
				array[num++] = f;
		}

		*faces = array;
		*numFaces = num;
		*freeFaces= 1;
	}
	else
		*freeFaces= 0;
}

static void ccgSubSurf__effectedFaceNeighbours(CCGSubSurf *ss, CCGFace **faces, int numFaces, CCGVert ***verts, int *numVerts, CCGEdge ***edges, int *numEdges)
{
	CCGVert **arrayV;
	CCGEdge **arrayE;
	int numV, numE, i, j;

	arrayV = MEM_mallocN(sizeof(*arrayV)*ss->vMap->numEntries, "CCGSubsurf arrayV");
	arrayE = MEM_mallocN(sizeof(*arrayE)*ss->eMap->numEntries, "CCGSubsurf arrayV");
	numV = numE = 0;

	for (i=0; i<numFaces; i++) {
		CCGFace *f = faces[i];
		f->flags |= Face_eEffected;
	}

	for (i=0; i<ss->vMap->curSize; i++) {
		CCGVert *v = (CCGVert*) ss->vMap->buckets[i];

		for (; v; v = v->next) {
			for(j=0; j<v->numFaces; j++)
				if(!(v->faces[j]->flags & Face_eEffected))
					break;
			
			if(j == v->numFaces) {
				arrayV[numV++] = v;
				v->flags |= Vert_eEffected;
			}
		}
	}

	for (i=0; i<ss->eMap->curSize; i++) {
		CCGEdge *e = (CCGEdge*) ss->eMap->buckets[i];

		for (; e; e = e->next) {
			for(j=0; j<e->numFaces; j++)
				if(!(e->faces[j]->flags & Face_eEffected))
					break;
			
			if(j == e->numFaces) {
				e->flags |= Edge_eEffected;
				arrayE[numE++] = e;
			}
		}
	}

	*verts = arrayV;
	*numVerts = numV;
	*edges = arrayE;
	*numEdges = numE;
}

/* copy face grid coordinates to other places */
CCGError ccgSubSurf_updateFromFaces(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	int i, S, x, gridSize, cornerIdx, subdivLevels;
	int vertDataSize = ss->meshIFC.vertDataSize, freeF;
	int finterpCount = ss->meshIFC.finterpCount;

	subdivLevels = ss->subdivLevels;
	lvl = (lvl)? lvl: subdivLevels;
	gridSize = 1 + (1<<(lvl-1));
	cornerIdx = gridSize-1;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);

	for (i=0; i<numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		for (S=0; S<f->numVerts; S++) {
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[(S+f->numVerts-1)%f->numVerts];

			VertDataCopy(FACE_getCenterData(f), FACE_getIFCo(f, lvl, S, 0, 0), finterpCount);
			VertDataCopy(VERT_getCo(FACE_getVerts(f)[S], lvl), FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), finterpCount);

			for (x=0; x<gridSize; x++)
				VertDataCopy(FACE_getIECo(f, lvl, S, x), FACE_getIFCo(f, lvl, S, x, 0), finterpCount);

			for (x=0; x<gridSize; x++) {
				int eI = gridSize-1-x;
				VertDataCopy(_edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI,vertDataSize), FACE_getIFCo(f, lvl, S, cornerIdx, x), finterpCount);
				VertDataCopy(_edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI,vertDataSize), FACE_getIFCo(f, lvl, S, x, cornerIdx), finterpCount);
			}
		}
	}

	if(freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* copy other places to face grid coordinates */
CCGError ccgSubSurf_updateToFaces(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	int i, S, x, gridSize, cornerIdx, subdivLevels;
	int vertDataSize = ss->meshIFC.vertDataSize, freeF;
	int finterpCount = ss->meshIFC.finterpCount;

	subdivLevels = ss->subdivLevels;
	lvl = (lvl)? lvl: subdivLevels;
	gridSize = 1 + (1<<(lvl-1));
	cornerIdx = gridSize-1;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);

	for (i=0; i<numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		for (S=0; S<f->numVerts; S++) {
			int prevS = (S+f->numVerts-1)%f->numVerts;
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[prevS];

			for (x=0; x<gridSize; x++) {
				int eI = gridSize-1-x;
				VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, x), _edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI,vertDataSize), finterpCount);
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, cornerIdx), _edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI,vertDataSize), finterpCount);
			}

			for (x=1; x<gridSize-1; x++) {
				VertDataCopy(FACE_getIFCo(f, lvl, S, 0, x), FACE_getIECo(f, lvl, prevS, x), finterpCount);
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, 0), FACE_getIECo(f, lvl, S, x), finterpCount);
			}

			VertDataCopy(FACE_getIFCo(f, lvl, S, 0, 0), FACE_getCenterData(f), finterpCount);
			VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), VERT_getCo(FACE_getVerts(f)[S], lvl), finterpCount);
		}
	}

	if(freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* stitch together face grids, averaging coordinates at edges
   and vertices, for multires displacements */
CCGError ccgSubSurf_stitchFaces(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	CCGVert **effectedV;
	CCGEdge **effectedE;
	int numEffectedV, numEffectedE, freeF;
	int i, S, x, gridSize, cornerIdx, subdivLevels, edgeSize;
	int vertDataSize = ss->meshIFC.vertDataSize;
	int finterpCount = ss->meshIFC.finterpCount;

	subdivLevels = ss->subdivLevels;
	lvl = (lvl)? lvl: subdivLevels;
	gridSize = 1 + (1<<(lvl-1));
	edgeSize = 1 + (1<<lvl);
	cornerIdx = gridSize-1;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);
	ccgSubSurf__effectedFaceNeighbours(ss, effectedF, numEffectedF,
		&effectedV, &numEffectedV, &effectedE, &numEffectedE);

	/* zero */
	for (i=0; i<numEffectedV; i++) {
		CCGVert *v = effectedV[i];
		VertDataZero(VERT_getCo(v, lvl), finterpCount);
	}

	for (i=0; i<numEffectedE; i++) {
		CCGEdge *e = effectedE[i];

		for (x=0; x<edgeSize; x++)
			VertDataZero(EDGE_getCo(e, lvl, x), finterpCount);
	}

	/* add */
	for (i=0; i<numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		VertDataZero(FACE_getCenterData(f), finterpCount);

		for (S=0; S<f->numVerts; S++)
			for (x=0; x<gridSize; x++)
				VertDataZero(FACE_getIECo(f, lvl, S, x), finterpCount);

		for (S=0; S<f->numVerts; S++) {
			int prevS = (S+f->numVerts-1)%f->numVerts;
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[prevS];

			VertDataAdd(FACE_getCenterData(f), FACE_getIFCo(f, lvl, S, 0, 0), finterpCount);
			if (FACE_getVerts(f)[S]->flags&Vert_eEffected)
				VertDataAdd(VERT_getCo(FACE_getVerts(f)[S], lvl), FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), finterpCount);

			for (x=1; x<gridSize-1; x++) {
				VertDataAdd(FACE_getIECo(f, lvl, S, x), FACE_getIFCo(f, lvl, S, x, 0), finterpCount);
				VertDataAdd(FACE_getIECo(f, lvl, prevS, x), FACE_getIFCo(f, lvl, S, 0, x), finterpCount);
			}

			for (x=0; x<gridSize-1; x++) {
				int eI = gridSize-1-x;
				if (FACE_getEdges(f)[S]->flags&Edge_eEffected)
					VertDataAdd(_edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI,vertDataSize), FACE_getIFCo(f, lvl, S, cornerIdx, x), finterpCount);
				if (FACE_getEdges(f)[prevS]->flags&Edge_eEffected)
					if(x != 0)
						VertDataAdd(_edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI,vertDataSize), FACE_getIFCo(f, lvl, S, x, cornerIdx), finterpCount);
			}
		}
	}

	/* average */
	for (i=0; i<numEffectedV; i++) {
		CCGVert *v = effectedV[i];
		VertDataMulN(VERT_getCo(v, lvl), 1.0f/v->numFaces, finterpCount);
	}

	for (i=0; i<numEffectedE; i++) {
		CCGEdge *e = effectedE[i];

		VertDataCopy(EDGE_getCo(e, lvl, 0), VERT_getCo(e->v0, lvl), finterpCount);
		VertDataCopy(EDGE_getCo(e, lvl, edgeSize-1), VERT_getCo(e->v1, lvl), finterpCount);

		for (x=1; x<edgeSize-1; x++)
			VertDataMulN(EDGE_getCo(e, lvl, x), 1.0f/e->numFaces, finterpCount);
	}

	/* copy */
	for (i=0; i<numEffectedF; i++) {
		CCGFace *f = effectedF[i];

		VertDataMulN(FACE_getCenterData(f), 1.0f/f->numVerts, finterpCount);

		for (S=0; S<f->numVerts; S++)
			for (x=1; x<gridSize-1; x++)
				VertDataMulN(FACE_getIECo(f, lvl, S, x), 0.5f, finterpCount);

		for (S=0; S<f->numVerts; S++) {
			int prevS = (S+f->numVerts-1)%f->numVerts;
			CCGEdge *e = FACE_getEdges(f)[S];
			CCGEdge *prevE = FACE_getEdges(f)[prevS];

			VertDataCopy(FACE_getIFCo(f, lvl, S, 0, 0), FACE_getCenterData(f), finterpCount);
			VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, cornerIdx), VERT_getCo(FACE_getVerts(f)[S], lvl), finterpCount);

			for (x=1; x<gridSize-1; x++) {
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, 0), FACE_getIECo(f, lvl, S, x), finterpCount);
				VertDataCopy(FACE_getIFCo(f, lvl, S, 0, x), FACE_getIECo(f, lvl, prevS, x), finterpCount);
			}

			for (x=0; x<gridSize-1; x++) {
				int eI = gridSize-1-x;
				VertDataCopy(FACE_getIFCo(f, lvl, S, cornerIdx, x), _edge_getCoVert(e, FACE_getVerts(f)[S], lvl, eI,vertDataSize), finterpCount);
				VertDataCopy(FACE_getIFCo(f, lvl, S, x, cornerIdx), _edge_getCoVert(prevE, FACE_getVerts(f)[S], lvl, eI,vertDataSize), finterpCount);
			}

			VertDataCopy(FACE_getIECo(f, lvl, S, 0), FACE_getCenterData(f), finterpCount);
			VertDataCopy(FACE_getIECo(f, lvl, S, gridSize-1), FACE_getIFCo(f, lvl, S, gridSize-1, 0), finterpCount);
		}
	}

	for (i=0; i<numEffectedV; i++)
		effectedV[i]->flags = 0;
	for (i=0; i<numEffectedE; i++)
		effectedE[i]->flags = 0;
	for (i=0; i<numEffectedF; i++)
		effectedF[i]->flags = 0;

	MEM_freeN(effectedE);
	MEM_freeN(effectedV);
	if(freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* update normals for specified faces */
CCGError ccgSubSurf_updateNormals(CCGSubSurf *ss, CCGFace **effectedF, int numEffectedF) {
	CCGVert **effectedV;
	CCGEdge **effectedE;
	int i, numEffectedV, numEffectedE, freeF;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);
	ccgSubSurf__effectedFaceNeighbours(ss, effectedF, numEffectedF,
		&effectedV, &numEffectedV, &effectedE, &numEffectedE);

	if (ss->calcVertNormals)
		ccgSubSurf__calcVertNormals(ss,
			effectedV, effectedE, effectedF,
			numEffectedV, numEffectedE, numEffectedF);

	for (i=0; i<numEffectedV; i++)
		effectedV[i]->flags = 0;
	for (i=0; i<numEffectedE; i++)
		effectedE[i]->flags = 0;
	for (i=0; i<numEffectedF; i++)
		effectedF[i]->flags = 0;

	MEM_freeN(effectedE);
	MEM_freeN(effectedV);
	if(freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

/* compute subdivision levels from a given starting point, used by
   multires subdivide/propagate, by filling in coordinates at a
   certain level, and then subdividing that up to the highest level */
CCGError ccgSubSurf_updateLevels(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
	CCGVert **effectedV;
	CCGEdge **effectedE;
	int numEffectedV, numEffectedE, freeF, i;
	int curLvl, subdivLevels = ss->subdivLevels;

	ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);
	ccgSubSurf__effectedFaceNeighbours(ss, effectedF, numEffectedF,
		&effectedV, &numEffectedV, &effectedE, &numEffectedE);

	for (curLvl=lvl; curLvl<subdivLevels; curLvl++) {
		ccgSubSurf__calcSubdivLevel(ss,
			effectedV, effectedE, effectedF,
			numEffectedV, numEffectedE, numEffectedF, curLvl);
	}

	for (i=0; i<numEffectedV; i++)
		effectedV[i]->flags = 0;
	for (i=0; i<numEffectedE; i++)
		effectedE[i]->flags = 0;
	for (i=0; i<numEffectedF; i++)
		effectedF[i]->flags = 0;

	MEM_freeN(effectedE);
	MEM_freeN(effectedV);
	if(freeF) MEM_freeN(effectedF);

	return eCCGError_None;
}

#undef VERT_getCo
#undef EDGE_getCo
#undef FACE_getIECo
#undef FACE_getIFCo

/*** External API accessor functions ***/

int ccgSubSurf_getNumVerts(CCGSubSurf *ss) {
	return ss->vMap->numEntries;
}
int ccgSubSurf_getNumEdges(CCGSubSurf *ss) {
	return ss->eMap->numEntries;
}
int ccgSubSurf_getNumFaces(CCGSubSurf *ss) {
	return ss->fMap->numEntries;
}

CCGVert *ccgSubSurf_getVert(CCGSubSurf *ss, CCGVertHDL v) {
	return (CCGVert*) _ehash_lookup(ss->vMap, v);
}
CCGEdge *ccgSubSurf_getEdge(CCGSubSurf *ss, CCGEdgeHDL e) {
	return (CCGEdge*) _ehash_lookup(ss->eMap, e);
}
CCGFace *ccgSubSurf_getFace(CCGSubSurf *ss, CCGFaceHDL f) {
	return (CCGFace*) _ehash_lookup(ss->fMap, f);
}

int ccgSubSurf_getSubdivisionLevels(CCGSubSurf *ss) {
	return ss->subdivLevels;
}
int ccgSubSurf_getEdgeSize(CCGSubSurf *ss) {
	return ccgSubSurf_getEdgeLevelSize(ss, ss->subdivLevels);
}
int ccgSubSurf_getEdgeLevelSize(CCGSubSurf *ss, int level) {
	if (level<1 || level>ss->subdivLevels) {
		return -1;
	} else {
		return 1 + (1<<level);
	}
}
int ccgSubSurf_getGridSize(CCGSubSurf *ss) {
	return ccgSubSurf_getGridLevelSize(ss, ss->subdivLevels);
}
int ccgSubSurf_getGridLevelSize(CCGSubSurf *ss, int level) {
	if (level<1 || level>ss->subdivLevels) {
		return -1;
	} else {
		return 1 + (1<<(level-1));
	}
}

struct GridKey *ccgSubSurf_getGridKey(CCGSubSurf *ss) {
	return &ss->meshIFC.gridkey;
}

/* Vert accessors */

CCGVertHDL ccgSubSurf_getVertVertHandle(CCGVert *v) {
	return v->vHDL;
}
int ccgSubSurf_getVertAge(CCGSubSurf *ss, CCGVert *v) {
	if (ss->useAgeCounts) {
		byte *userData = ccgSubSurf_getVertUserData(ss, v);
		return ss->currentAge - *((int*) &userData[ss->vertUserAgeOffset]);
	} else {
		return 0;
	}
}
void *ccgSubSurf_getVertUserData(CCGSubSurf *ss, CCGVert *v) {
	return VERT_getLevelData(v) + ss->meshIFC.vertDataSize*(ss->subdivLevels+1);
}
int ccgSubSurf_getVertNumFaces(CCGVert *v) {
	return v->numFaces;
}
CCGFace *ccgSubSurf_getVertFace(CCGVert *v, int index) {
	if (index<0 || index>=v->numFaces) {
		return NULL;
	} else {
		return v->faces[index];
	}
}
int ccgSubSurf_getVertNumEdges(CCGVert *v) {
	return v->numEdges;
}
CCGEdge *ccgSubSurf_getVertEdge(CCGVert *v, int index) {
	if (index<0 || index>=v->numEdges) {
		return NULL;
	} else {
		return v->edges[index];
	}
}
void *ccgSubSurf_getVertData(CCGSubSurf *ss, CCGVert *v) {
	return ccgSubSurf_getVertLevelData(ss, v, ss->subdivLevels);
}
void *ccgSubSurf_getVertLevelData(CCGSubSurf *ss, CCGVert *v, int level) {
	if (level<0 || level>ss->subdivLevels) {
		return NULL;
	} else {
		return _vert_getCo(v, level, ss->meshIFC.vertDataSize);
	}
}

/* Edge accessors */

CCGEdgeHDL ccgSubSurf_getEdgeEdgeHandle(CCGEdge *e) {
	return e->eHDL;
}
int ccgSubSurf_getEdgeAge(CCGSubSurf *ss, CCGEdge *e) {
	if (ss->useAgeCounts) {
		byte *userData = ccgSubSurf_getEdgeUserData(ss, e);
		return ss->currentAge - *((int*) &userData[ss->edgeUserAgeOffset]);
	} else {
		return 0;
	}
}
void *ccgSubSurf_getEdgeUserData(CCGSubSurf *ss, CCGEdge *e) {
	return EDGE_getLevelData(e) + ss->meshIFC.vertDataSize *((ss->subdivLevels+1) + (1<<(ss->subdivLevels+1))-1);
}
int ccgSubSurf_getEdgeNumFaces(CCGEdge *e) {
	return e->numFaces;
}
CCGFace *ccgSubSurf_getEdgeFace(CCGEdge *e, int index) {
	if (index<0 || index>=e->numFaces) {
		return NULL;
	} else {
		return e->faces[index];
	}
}
CCGVert *ccgSubSurf_getEdgeVert0(CCGEdge *e) {
	return e->v0;
}
CCGVert *ccgSubSurf_getEdgeVert1(CCGEdge *e) {
	return e->v1;
}
void *ccgSubSurf_getEdgeDataArray(CCGSubSurf *ss, CCGEdge *e) {
	return ccgSubSurf_getEdgeData(ss, e, 0);
}
void *ccgSubSurf_getEdgeData(CCGSubSurf *ss, CCGEdge *e, int x) {
	return ccgSubSurf_getEdgeLevelData(ss, e, x, ss->subdivLevels);
}
void *ccgSubSurf_getEdgeLevelData(CCGSubSurf *ss, CCGEdge *e, int x, int level) {
	if (level<0 || level>ss->subdivLevels) {
		return NULL;
	} else {
		return _edge_getCo(e, level, x, ss->meshIFC.vertDataSize);
	}
}
float ccgSubSurf_getEdgeCrease(CCGEdge *e) {
	return e->crease;
}

/* Face accessors */

CCGFaceHDL ccgSubSurf_getFaceFaceHandle(CCGSubSurf *UNUSED(ss), CCGFace *f) {
	return f->fHDL;
}
int ccgSubSurf_getFaceAge(CCGSubSurf *ss, CCGFace *f) {
	if (ss->useAgeCounts) {
		byte *userData = ccgSubSurf_getFaceUserData(ss, f);
		return ss->currentAge - *((int*) &userData[ss->faceUserAgeOffset]);
	} else {
		return 0;
	}
}
void *ccgSubSurf_getFaceUserData(CCGSubSurf *ss, CCGFace *f) {
	int maxGridSize = 1 + (1<<(ss->subdivLevels-1));
	return FACE_getCenterData(f) + ss->meshIFC.vertDataSize *(1 + f->numVerts*maxGridSize + f->numVerts*maxGridSize*maxGridSize);
}
int ccgSubSurf_getFaceNumVerts(CCGFace *f) {
	return f->numVerts;
}
CCGVert *ccgSubSurf_getFaceVert(CCGSubSurf *UNUSED(ss), CCGFace *f, int index) {
	if (index<0 || index>=f->numVerts) {
		return NULL;
	} else {
		return FACE_getVerts(f)[index];
	}
}
CCGEdge *ccgSubSurf_getFaceEdge(CCGSubSurf *UNUSED(ss), CCGFace *f, int index) {
	if (index<0 || index>=f->numVerts) {
		return NULL;
	} else {
		return FACE_getEdges(f)[index];
	}
}
int ccgSubSurf_getFaceEdgeIndex(CCGFace *f, CCGEdge *e) {
	int i;

	for (i=0; i<f->numVerts; i++)
		if (FACE_getEdges(f)[i]==e)
			return i;

	return -1;
}
void *ccgSubSurf_getFaceCenterData(CCGFace *f) {
	return FACE_getCenterData(f);
}
void *ccgSubSurf_getFaceGridEdgeDataArray(CCGSubSurf *ss, CCGFace *f, int gridIndex) {
	return ccgSubSurf_getFaceGridEdgeData(ss, f, gridIndex, 0);
}
void *ccgSubSurf_getFaceGridEdgeData(CCGSubSurf *ss, CCGFace *f, int gridIndex, int x) {
	return _face_getIECo(f, ss->subdivLevels, gridIndex, x, ss->subdivLevels, ss->meshIFC.vertDataSize);
}
void *ccgSubSurf_getFaceGridDataArray(CCGSubSurf *ss, CCGFace *f, int gridIndex) {
	return ccgSubSurf_getFaceGridData(ss, f, gridIndex, 0, 0);
}
void *ccgSubSurf_getFaceGridData(CCGSubSurf *ss, CCGFace *f, int gridIndex, int x, int y) {
	return _face_getIFCo(f, ss->subdivLevels, gridIndex, x, y, ss->subdivLevels, ss->meshIFC.vertDataSize);
}

/*** External API iterator functions ***/

CCGVertIterator *ccgSubSurf_getVertIterator(CCGSubSurf *ss) {
	return (CCGVertIterator*) _ehashIterator_new(ss->vMap);
}
CCGEdgeIterator *ccgSubSurf_getEdgeIterator(CCGSubSurf *ss) {
	return (CCGEdgeIterator*) _ehashIterator_new(ss->eMap);
}
CCGFaceIterator *ccgSubSurf_getFaceIterator(CCGSubSurf *ss) {
	return (CCGFaceIterator*) _ehashIterator_new(ss->fMap);
}

CCGVert *ccgVertIterator_getCurrent(CCGVertIterator *vi) {
	return (CCGVert*) _ehashIterator_getCurrent((EHashIterator*) vi);
}
int ccgVertIterator_isStopped(CCGVertIterator *vi) {
	return _ehashIterator_isStopped((EHashIterator*) vi);
}
void ccgVertIterator_next(CCGVertIterator *vi) {
	_ehashIterator_next((EHashIterator*) vi); 
}
void ccgVertIterator_free(CCGVertIterator *vi) {
	_ehashIterator_free((EHashIterator*) vi);
}

CCGEdge *ccgEdgeIterator_getCurrent(CCGEdgeIterator *vi) {
	return (CCGEdge*) _ehashIterator_getCurrent((EHashIterator*) vi);
}
int ccgEdgeIterator_isStopped(CCGEdgeIterator *vi) {
	return _ehashIterator_isStopped((EHashIterator*) vi);
}
void ccgEdgeIterator_next(CCGEdgeIterator *vi) {
	_ehashIterator_next((EHashIterator*) vi); 
}
void ccgEdgeIterator_free(CCGEdgeIterator *vi) {
	_ehashIterator_free((EHashIterator*) vi);
}

CCGFace *ccgFaceIterator_getCurrent(CCGFaceIterator *vi) {
	return (CCGFace*) _ehashIterator_getCurrent((EHashIterator*) vi);
}
int ccgFaceIterator_isStopped(CCGFaceIterator *vi) {
	return _ehashIterator_isStopped((EHashIterator*) vi);
}
void ccgFaceIterator_next(CCGFaceIterator *vi) {
	_ehashIterator_next((EHashIterator*) vi); 
}
void ccgFaceIterator_free(CCGFaceIterator *vi) {
	_ehashIterator_free((EHashIterator*) vi);
}

/*** Extern API final vert/edge/face interface ***/

int ccgSubSurf_getNumFinalVerts(CCGSubSurf *ss) {
	int edgeSize = 1 + (1<<ss->subdivLevels);
	int gridSize = 1 + (1<<(ss->subdivLevels-1));
	int numFinalVerts = ss->vMap->numEntries + ss->eMap->numEntries*(edgeSize-2) + ss->fMap->numEntries + ss->numGrids*((gridSize-2) + ((gridSize-2)*(gridSize-2)));
	return numFinalVerts;
}
int ccgSubSurf_getNumFinalEdges(CCGSubSurf *ss) {
	int edgeSize = 1 + (1<<ss->subdivLevels);
	int gridSize = 1 + (1<<(ss->subdivLevels-1));
	int numFinalEdges = ss->eMap->numEntries*(edgeSize-1) + ss->numGrids*((gridSize-1) + 2*((gridSize-2)*(gridSize-1)));
	return numFinalEdges;
}
int ccgSubSurf_getNumFinalFaces(CCGSubSurf *ss) {
	int gridSize = 1 + (1<<(ss->subdivLevels-1));
	int numFinalFaces = ss->numGrids*((gridSize-1)*(gridSize-1));
	return numFinalFaces;
}
