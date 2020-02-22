/*
** dthinker.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __DTHINKER_H__
#define __DTHINKER_H__

#include <stdlib.h>
#include "dobject.h"
#include "statnums.h"

class AActor;
class player_t;
struct pspdef_s;
struct FState;
class DThinker;
class FSerializer;

class FThinkerIterator;

enum { MAX_STATNUM = 127 };

// Doubly linked ring list of thinkers
struct FThinkerList
{
	FThinkerList() : Sentinel(0) {}
	void AddTail(DThinker *thinker);
	DThinker *GetHead() const;
	DThinker *GetTail() const;
	bool IsEmpty() const;

	DThinker *Sentinel;
};

class DThinker : public DObject
{
	DECLARE_CLASS (DThinker, DObject)
public:
	DThinker (int statnum = STAT_DEFAULT) throw();
	void OnDestroy () override;
	virtual ~DThinker ();
	virtual void Tick ();
	void CallTick();
	virtual void PostBeginPlay ();	// Called just before the first tick
	virtual void CallPostBeginPlay(); // different in actor.
	virtual void PostSerialize();
	size_t PropagateMark();
	
	void ChangeStatNum (int statnum);

	static void RunThinkers ();
	static void RunThinkers (int statnum);
	static void DestroyAllThinkers ();
	static void DestroyThinkersInList(int statnum)
	{
		DestroyThinkersInList(Thinkers[statnum]);
		DestroyThinkersInList(FreshThinkers[statnum]);
	}
	static void SerializeThinkers(FSerializer &arc, bool keepPlayers);
	static void MarkRoots();

	static DThinker *FirstThinker (int statnum);
	static bool bSerialOverride;

	// only used internally but Create needs access.
	enum no_link_type { NO_LINK };
	DThinker(no_link_type) throw();
private:
	static void DestroyThinkersInList (FThinkerList &list);
	static bool DoDestroyThinkersInList(FThinkerList &list);
	static int TickThinkers (FThinkerList *list, FThinkerList *dest);	// Returns: # of thinkers ticked
	static int ProfileThinkers(FThinkerList *list, FThinkerList *dest);
	static void SaveList(FSerializer &arc, DThinker *node);
	void Remove();

	static FThinkerList Thinkers[MAX_STATNUM+2];		// Current thinkers
	static FThinkerList FreshThinkers[MAX_STATNUM+1];	// Newly created thinkers

	friend struct FThinkerList;
	friend class FThinkerIterator;
	friend class DObject;
	friend class FSerializer;

	DThinker *NextThinker, *PrevThinker;
};

class FThinkerIterator
{
protected:
	const PClass *m_ParentType;
private:
	DThinker *m_CurrThinker;
	uint8_t m_Stat;
	bool m_SearchStats;
	bool m_SearchingFresh;

public:
	FThinkerIterator (const PClass *type, int statnum=MAX_STATNUM+1);
	FThinkerIterator (const PClass *type, int statnum, DThinker *prev);
	DThinker *Next (bool exact = false);
	void Reinit ();
};

template <class T> class TThinkerIterator : public FThinkerIterator
{
public:
	TThinkerIterator (int statnum=MAX_STATNUM+1) : FThinkerIterator (RUNTIME_CLASS(T), statnum)
	{
	}
	TThinkerIterator (int statnum, DThinker *prev) : FThinkerIterator (RUNTIME_CLASS(T), statnum, prev)
	{
	}
	TThinkerIterator (const PClass *subclass, int statnum=MAX_STATNUM+1) : FThinkerIterator(subclass, statnum)
	{
	}
	TThinkerIterator (FName subclass, int statnum=MAX_STATNUM+1) : FThinkerIterator(PClass::FindClass(subclass), statnum)
	{
	}
	TThinkerIterator (ENamedName subclass, int statnum=MAX_STATNUM+1) : FThinkerIterator(PClass::FindClass(subclass), statnum)
	{
	}
	TThinkerIterator (FName subclass, int statnum, DThinker *prev) : FThinkerIterator(PClass::FindClass(subclass), statnum, prev)
	{
	}
	TThinkerIterator (ENamedName subclass, int statnum, DThinker *prev) : FThinkerIterator(PClass::FindClass(subclass), statnum, prev)
	{
	}
	TThinkerIterator (const char *subclass, int statnum=MAX_STATNUM+1) : FThinkerIterator(PClass::FindClass(subclass), statnum)
	{
	}
	T *Next (bool exact = false)
	{
		return static_cast<T *>(FThinkerIterator::Next (exact));
	}
};

#endif //__DTHINKER_H__
