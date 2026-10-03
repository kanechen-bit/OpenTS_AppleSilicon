/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

template<class T>
class TClassFactory : public IClassFactory
{
	public:
		TClassFactory(void);

		STDMETHOD(QueryInterface)(REFIID riid, void **ppvObj);
		STDMETHOD_(ULONG, AddRef)(void);
		STDMETHOD_(ULONG, Release)(void);

		STDMETHOD(CreateInstance)(IUnknown *pUnkOuter, REFIID riid, void **ppbObj);
		STDMETHOD(LockServer)(BOOL fLock);

	private:
		/*
		 * This is the number of outstanding references to this factory, counting both the
		 * interface pointers handed out and any server locks taken. The factory deletes
		 * itself once the count falls back to zero.
		 */
		LONG RefCount;
};


template<class T>
TClassFactory<T>::TClassFactory(void) :
	RefCount(0)
{
}


template<class T>
STDMETHODIMP TClassFactory<T>::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == NULL) {
		return(E_POINTER);
	}

	*ppvObj = NULL;

	if (riid == IID_IUnknown) {
		*ppvObj = (void *)((IClassFactory *)this);
	} else if (riid == IID_IClassFactory) {
		*ppvObj = (void *)((IClassFactory *)this);
	}

	if (*ppvObj == NULL) {
		return(E_NOINTERFACE);
	}

	((IClassFactory *)this)->AddRef();

	return(S_OK);
}


template<class T>
ULONG TClassFactory<T>::AddRef(void)
{
	return(InterlockedIncrement(&RefCount));
}


template<class T>
ULONG TClassFactory<T>::Release(void)
{
	int count = InterlockedDecrement(&RefCount);
	if (count == 0) {
		delete this;
	}

	return(count);
}


template<class T>
STDMETHODIMP TClassFactory<T>::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObj)
{
	if (ppvObj == NULL) {
		return(E_INVALIDARG);
	}

	*ppvObj = NULL;
	if (pUnkOuter != NULL) {
		return(CLASS_E_NOAGGREGATION);
	}

	T *obj = new T();
	if (obj == NULL) {
		return(E_OUTOFMEMORY);
	}

	HRESULT hr = obj->QueryInterface(riid, ppvObj);
	if (FAILED(hr)) {
		delete obj;
	}

	return(hr);
}


template<class T>
HRESULT STDMETHODCALLTYPE TClassFactory<T>::LockServer(BOOL fLock)
{
	if (fLock) {
		RefCount++;
	} else {
		RefCount--;
	}
	return(S_OK);
}
