/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "vector.h"


/*
 * A single key/value entry stored within a hash table bucket. Equality and
 * the "empty" test are keyed off the Key alone.
 */
template<typename K, typename V>
struct HashObject {
		HashObject(void) {}
		HashObject(K const & key, V const & value) : Key(key), Value(value) {}
		HashObject(HashObject const & that) : Key(that.Key), Value(that.Value) {}

		bool operator!=(const HashObject & that) const { return(Key != that.Key); }
		bool operator==(const HashObject & that) const { return(Key == that.Key); }

		bool operator!() const { return(Key == 0); }

		/*
		 * This is the key the entry is filed under. It carries the entry's whole identity,
		 * so a bucket never holds two entries whose keys match.
		 */
		K Key;

		/*
		 * This is the value filed under the key. A table with nothing of its own to store
		 * packs everything it needs into the key and merely repeats it here.
		 */
		V Value;
};


/*
 * A chained (separate-chaining) hash table. It owns a fixed array of
 * NumBuckets buckets, each bucket being a growable vector of key/value
 * pairs. Collisions within a bucket are resolved by a linear scan.
 * The same template backs every hash table in the game -- the map's
 * subzone/zone connection tables and the radar object tracking table --
 * which differ only in their key/value types and in how the owning class
 * derives a bucket index for a given key. Because of that, two styles of
 * insertion coexist:
 * * The caller computes the bucket index itself and passes it in
 * (the subzone connection tables, whose keys are packed zone ids).
 * * The table hashes the key directly via Key.Hash() and picks head or
 * tail placement via Key.Use_Head() (the radar tracking table).
 * Only the members actually used by a given instantiation are generated,
 * so a key type need not provide Hash()/Use_Head() unless the key-hashed
 * overloads are called for it.
 * The owning class is a friend and also reaches into Buckets directly for
 * iteration and some lookups.
 */
template<typename K, typename V>
class HashTableClass
{
	public:
		friend class RadarClass;

		typedef HashObject<K, V> ObjectType;
		typedef DynamicVectorClass<ObjectType> BucketType;
		typedef int (*HASH_FUNC)(const K &);

	public:
		HashTableClass(int growth_step, int num_buckets, HASH_FUNC hash_func);
		~HashTableClass(void);

		/*
		 * Caller-hashed insertion. The packed key supplies its own bucket
		 * index, or the index is passed in explicitly.
		 */
		bool Add_Object(ObjectType const & object);
		bool Add_Object(int bucket_index, ObjectType const & object);

		/*
		 * Key-hashed insertion: bucket index comes from Key.Hash() and the
		 * entry is placed at the head or tail based on Key.Use_Head().
		 */
		bool Add_Object(ObjectType const & o, bool head);

		bool Get(const K & key, V & out);
		bool Remove_Object(const K & key, const V & value);

		void Destroy(void);
		void Clear(void);

	public:
		/*
		 * The array of NumBuckets buckets.
		 */
		BucketType * Buckets;

		/*
		 * A pointer to the function for hashing an item's key. Nothing calls
		 * through it -- the bucket index is derived at each call site instead.
		 */
		HASH_FUNC HashFunction;

		/*
		 * The number of buckets in the hash table.
		 */
		int NumBuckets;

		/*
		 * When a bucket has insufficient room left, it will grow by the number
		 * of objects specified by this value.
		 */
		int GrowthStep;
};


template<typename K, typename V>
inline HashTableClass<K, V>::HashTableClass(int growth_step, int num_buckets, HASH_FUNC hash_func) :
	HashFunction(hash_func),
	NumBuckets(num_buckets),
	GrowthStep(growth_step)
{
	Buckets = new BucketType[num_buckets];
	for (int index = 0; index < NumBuckets; index++) {
		Buckets[index].Set_Growth_Step(growth_step);
	}
}


template<typename K, typename V>
inline HashTableClass<K, V>::~HashTableClass(void)
{
	Destroy();
}


template<typename K, typename V>
inline bool HashTableClass<K, V>::Add_Object(ObjectType const & object)
{
	unsigned packed = object.Value | (object.Key << 16);
	BucketType & bucket = Buckets[object.Value & 0xF | ((object.Key & 0xF) << 4)];

	if (bucket.Count() > 0) {
		ObjectType * obj = &bucket[0];
		for (int index = bucket.Count() - 1; index >= 0; index--) {
			if (ObjectType(packed, packed) == *obj) {
				return(false);
			}
			obj++;
		}
	}

	return(bucket.Add(ObjectType(packed, packed)));
}


template<typename K, typename V>
inline bool HashTableClass<K, V>::Add_Object(int bucket_index, ObjectType const & object)
{
	BucketType & bucket = Buckets[bucket_index];

	if (bucket.Count() > 0) {
		ObjectType * obj = &bucket[0];
		for (int index = bucket.Count() - 1; index >= 0; index--) {
			if ((K &)object == (K &)*obj) {

				return(false);
			}
			obj++;
		}
	}
	bucket.Add(object);
	return(true);
}


template<typename K, typename V>
inline bool HashTableClass<K, V>::Add_Object(const ObjectType & o, bool head)
{
	BucketType & bucket = Buckets[o.Key.Hash()];
	if (bucket.Count() > 0) {
		const ObjectType * oo = &bucket[0];

		for (int index = 0; index < bucket.Count(); index++) {
			if (oo[index] == o) {
				return(false);
			}
		}
	}

	if (o.Key.Use_Head()) {
		bucket.Add_Head(o);
	} else {
		bucket.Add(o);
	}
	return(true);
}


template<typename K, typename V>
inline bool HashTableClass<K, V>::Get(const K & key, V & out)
{
	BucketType & bucket = Buckets[key.Hash()];
	int index = bucket.Count() - 1;
	out = V();

	if (index >= 0) {
		ObjectType * oo = &bucket[0];
		while (true) {
			V value = (V &)bucket[index].Key;
			if ((K &)oo[index].Key == (K &)key) {
				out = value;
				return(true);
			}
			index--;
			if (index < 0) {
				break;
			}
		}
	}

	return(false);
}


template<typename K, typename V>
inline bool HashTableClass<K, V>::Remove_Object(const K & key, const V & value)
{
	BucketType * bucket = &Buckets[key.Hash()];
	ObjectType a(key, value);

	return(bucket->Delete(a));
}


template<typename K, typename V>
inline void HashTableClass<K, V>::Destroy(void)
{
	delete[] Buckets;
}


template<typename K, typename V>
inline void HashTableClass<K, V>::Clear(void)
{
	for (int index = 0; index < NumBuckets; index++) {
		Buckets[index].Clear();
	}
}
