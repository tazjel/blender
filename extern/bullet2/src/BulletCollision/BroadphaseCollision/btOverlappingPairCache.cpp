
/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/



#include "btOverlappingPairCache.h"

#include "btDispatcher.h"
#include "btCollisionAlgorithm.h"

int	gOverlappingPairs = 0;

int gRemovePairs =0;
int gAddedPairs =0;
int gFindPairs =0;

btOverlappingPairCache::btOverlappingPairCache():
	m_overlapFilterCallback(0),
	m_blockedForChanges(false)
{
	int initialAllocatedSize= 2;
	m_overlappingPairArray.reserve(initialAllocatedSize);
#ifdef USE_HASH_PAIRCACHE
	growTables();
#endif //USE_HASH_PAIRCACHE
}


btOverlappingPairCache::~btOverlappingPairCache()
{
	//todo/test: show we erase/delete data, or is it automatic
}

void	btOverlappingPairCache::cleanOverlappingPair(btBroadphasePair& pair,btDispatcher* dispatcher)
{
	if (pair.m_algorithm)
	{
		{
			pair.m_algorithm->~btCollisionAlgorithm();
			dispatcher->freeCollisionAlgorithm(pair.m_algorithm);
			pair.m_algorithm=0;
		}
	}
}


void	btOverlappingPairCache::cleanProxyFromPairs(btBroadphaseProxy* proxy,btDispatcher* dispatcher)
{

	class	CleanPairCallback : public btOverlapCallback
	{
		btBroadphaseProxy* m_cleanProxy;
		btOverlappingPairCache*	m_pairCache;
		btDispatcher* m_dispatcher;

	public:
		CleanPairCallback(btBroadphaseProxy* cleanProxy,btOverlappingPairCache* pairCache,btDispatcher* dispatcher)
			:m_cleanProxy(cleanProxy),
			m_pairCache(pairCache),
			m_dispatcher(dispatcher)
		{
		}
		virtual	bool	processOverlap(btBroadphasePair& pair)
		{
			if ((pair.m_pProxy0 == m_cleanProxy) ||
				(pair.m_pProxy1 == m_cleanProxy))
			{
				m_pairCache->cleanOverlappingPair(pair,m_dispatcher);
			}
			return false;
		}
		
	};

	CleanPairCallback cleanPairs(proxy,this,dispatcher);

	processAllOverlappingPairs(&cleanPairs,dispatcher);

}

void	btOverlappingPairCache::removeOverlappingPairsContainingProxy(btBroadphaseProxy* proxy,btDispatcher* dispatcher)
{

	class	RemovePairCallback : public btOverlapCallback
	{
		btBroadphaseProxy* m_obsoleteProxy;

	public:
		RemovePairCallback(btBroadphaseProxy* obsoleteProxy)
			:m_obsoleteProxy(obsoleteProxy)
		{
		}
		virtual	bool	processOverlap(btBroadphasePair& pair)
		{
			return ((pair.m_pProxy0 == m_obsoleteProxy) ||
				(pair.m_pProxy1 == m_obsoleteProxy));
		}
		
	};


	RemovePairCallback removeCallback(proxy);

	processAllOverlappingPairs(&removeCallback,dispatcher);
}


#ifdef USE_HASH_PAIRCACHE







btBroadphasePair* btOverlappingPairCache::findPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1)
{
	gFindPairs++;

	int proxyId1 = proxy0->getUid();
	int proxyId2 = proxy1->getUid();

	if (proxyId1 > proxyId2) 
		btSwap(proxyId1, proxyId2);

	int hash = getHash(proxyId1, proxyId2) & (m_overlappingPairArray.capacity()-1);

	int index = m_hashTable[hash];
	while (index != BT_NULL_PAIR && equalsPair(m_overlappingPairArray[index], proxyId1, proxyId2) == false)
	{
		index = m_next[index];
	}

	if (index == BT_NULL_PAIR)
	{
		return NULL;
	}

	btAssert(index < m_overlappingPairArray.size());

	return &m_overlappingPairArray[index];
}

#include <stdio.h>

void	btOverlappingPairCache::growTables()
{

	int newCapacity = m_overlappingPairArray.capacity();

	if (m_hashTable.size() < newCapacity)
	{
		//grow hashtable and next table
		int curHashtableSize = m_hashTable.size();

		m_hashTable.resize(newCapacity);
		m_next.resize(newCapacity);


		int i;

		for (i= 0; i < newCapacity; ++i)
		{
			m_hashTable[i] = BT_NULL_PAIR;
		}
		for (i = 0; i < newCapacity; ++i)
		{
			m_next[i] = BT_NULL_PAIR;
		}

		for(i=0;i<curHashtableSize;i++)
		{
	
			const btBroadphasePair& pair = m_overlappingPairArray[i];
			int proxyId1 = pair.m_pProxy0->getUid();
			int proxyId2 = pair.m_pProxy1->getUid();
			if (proxyId1 > proxyId2) 
				btSwap(proxyId1, proxyId2);
			int	hashValue = getHash(proxyId1,proxyId2) & (m_overlappingPairArray.capacity()-1);	// New hash value with new mask
			m_next[i] = m_hashTable[hashValue];
			m_hashTable[hashValue] = i;
		}


	}
}

btBroadphasePair* btOverlappingPairCache::internalAddPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1)
{
	int proxyId1 = proxy0->getUid();
	int proxyId2 = proxy1->getUid();

	if (proxyId1 > proxyId2) 
		btSwap(proxyId1, proxyId2);

	int hash = getHash(proxyId1, proxyId2) & (m_overlappingPairArray.capacity()-1);

	

	btBroadphasePair* pair = internalFindPair(proxy0, proxy1, hash);
	if (pair != NULL)
	{
		return pair;
	}

	int count = m_overlappingPairArray.size();
	int oldCapacity = m_overlappingPairArray.capacity();
	void* mem = &m_overlappingPairArray.expand();
	int newCapacity = m_overlappingPairArray.capacity();

	if (oldCapacity < newCapacity)
	{
		growTables();
		//hash with new capacity
		hash = getHash(proxyId1, proxyId2) & (m_overlappingPairArray.capacity()-1);
	}
	
	pair = new (mem) btBroadphasePair(*proxy0,*proxy1);
//	pair->m_pProxy0 = proxy0;
//	pair->m_pProxy1 = proxy1;
	pair->m_algorithm = 0;
	pair->m_userInfo = 0;
	

	m_next[count] = m_hashTable[hash];
	m_hashTable[hash] = count;

	return pair;
}



void* btOverlappingPairCache::removeOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1,btDispatcher* dispatcher)
{
	gRemovePairs++;

	int proxyId1 = proxy0->getUid();
	int proxyId2 = proxy1->getUid();

	if (proxyId1 > proxyId2) 
		btSwap(proxyId1, proxyId2);

	int hash = getHash(proxyId1, proxyId2) & (m_overlappingPairArray.capacity()-1);

	btBroadphasePair* pair = internalFindPair(proxy0, proxy1, hash);
	if (pair == NULL)
	{
		return 0;
	}

	cleanOverlappingPair(*pair,dispatcher);

	void* userData = pair->m_userInfo;

	btAssert(pair->m_pProxy0->getUid() == proxyId1);
	btAssert(pair->m_pProxy1->getUid() == proxyId2);

	int pairIndex = int(pair - &m_overlappingPairArray[0]);
	btAssert(pairIndex < m_overlappingPairArray.size());

	// Remove the pair from the hash table.
	int index = m_hashTable[hash];
	btAssert(index != BT_NULL_PAIR);

	int previous = BT_NULL_PAIR;
	while (index != pairIndex)
	{
		previous = index;
		index = m_next[index];
	}

	if (previous != BT_NULL_PAIR)
	{
		btAssert(m_next[previous] == pairIndex);
		m_next[previous] = m_next[pairIndex];
	}
	else
	{
		m_hashTable[hash] = m_next[pairIndex];
	}

	// We now move the last pair into spot of the
	// pair being removed. We need to fix the hash
	// table indices to support the move.

	int lastPairIndex = m_overlappingPairArray.size() - 1;

	// If the removed pair is the last pair, we are done.
	if (lastPairIndex == pairIndex)
	{
		m_overlappingPairArray.pop_back();
		return userData;
	}

	// Remove the last pair from the hash table.
	const btBroadphasePair* last = &m_overlappingPairArray[lastPairIndex];
	int lastHash = getHash(last->m_pProxy0->getUid(), last->m_pProxy1->getUid()) & (m_overlappingPairArray.capacity()-1);

	index = m_hashTable[lastHash];
	btAssert(index != BT_NULL_PAIR);

	previous = BT_NULL_PAIR;
	while (index != lastPairIndex)
	{
		previous = index;
		index = m_next[index];
	}

	if (previous != BT_NULL_PAIR)
	{
		btAssert(m_next[previous] == lastPairIndex);
		m_next[previous] = m_next[lastPairIndex];
	}
	else
	{
		m_hashTable[lastHash] = m_next[lastPairIndex];
	}

	// Copy the last pair into the remove pair's spot.
	m_overlappingPairArray[pairIndex] = m_overlappingPairArray[lastPairIndex];

	// Insert the last pair into the hash table
	m_next[pairIndex] = m_hashTable[lastHash];
	m_hashTable[lastHash] = pairIndex;

	m_overlappingPairArray.pop_back();

	return userData;
}
#include <stdio.h>

void	btOverlappingPairCache::processAllOverlappingPairs(btOverlapCallback* callback,btDispatcher* dispatcher)
{

	int i;

//	printf("m_overlappingPairArray.size()=%d\n",m_overlappingPairArray.size());
	for (i=0;i<m_overlappingPairArray.size();)
	{
	
		btBroadphasePair* pair = &m_overlappingPairArray[i];
		if (callback->processOverlap(*pair))
		{
			removeOverlappingPair(pair->m_pProxy0,pair->m_pProxy1,dispatcher);

			gOverlappingPairs--;
		} else
		{
			i++;
		}
	}
}

#else




void*	btOverlappingPairCache::removeOverlappingPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1, btDispatcher* dispatcher )
{
#ifndef USE_LAZY_REMOVAL

	btBroadphasePair findPair(*proxy0,*proxy1);

	int findIndex = m_overlappingPairArray.findLinearSearch(findPair);
	if (findIndex < m_overlappingPairArray.size())
	{
		gOverlappingPairs--;
		btBroadphasePair& pair = m_overlappingPairArray[findIndex];
		void* userData = pair.m_userInfo;
		cleanOverlappingPair(pair,dispatcher);
		
		m_overlappingPairArray.swap(findIndex,m_overlappingPairArray.capacity()-1);
		m_overlappingPairArray.pop_back();
		return userData;
	}
#endif //USE_LAZY_REMOVAL

	return 0;
}








btBroadphasePair*	btOverlappingPairCache::addOverlappingPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1)
{
	//don't add overlap with own
	assert(proxy0 != proxy1);

	if (!needsBroadphaseCollision(proxy0,proxy1))
		return 0;
	
	void* mem = &m_overlappingPairArray.expand();
	btBroadphasePair* pair = new (mem) btBroadphasePair(*proxy0,*proxy1);
	gOverlappingPairs++;
	return pair;
	
}

///this findPair becomes really slow. Either sort the list to speedup the query, or
///use a different solution. It is mainly used for Removing overlapping pairs. Removal could be delayed.
///we could keep a linked list in each proxy, and store pair in one of the proxies (with lowest memory address)
///Also we can use a 2D bitmap, which can be useful for a future GPU implementation
 btBroadphasePair*	btOverlappingPairCache::findPair(btBroadphaseProxy* proxy0,btBroadphaseProxy* proxy1)
{
	if (!needsBroadphaseCollision(proxy0,proxy1))
		return 0;

	btBroadphasePair tmpPair(*proxy0,*proxy1);
	int findIndex = m_overlappingPairArray.findLinearSearch(tmpPair);

	if (findIndex < m_overlappingPairArray.size())
	{
		//assert(it != m_overlappingPairSet.end());
		 btBroadphasePair* pair = &m_overlappingPairArray[findIndex];
		return pair;
	}
	return 0;
}










#include <stdio.h>

void	btOverlappingPairCache::processAllOverlappingPairs(btOverlapCallback* callback,btDispatcher* dispatcher)
{

	int i;

	for (i=0;i<m_overlappingPairArray.size();)
	{
	
		btBroadphasePair* pair = &m_overlappingPairArray[i];
		if (callback->processOverlap(*pair))
		{
			cleanOverlappingPair(*pair,dispatcher);

			m_overlappingPairArray.swap(i,m_overlappingPairArray.capacity()-1);
			m_overlappingPairArray.pop_back();
			gOverlappingPairs--;
		} else
		{
			i++;
		}
	}
}



#endif //USE_HASH_PAIRCACHE
