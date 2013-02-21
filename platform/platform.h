#ifndef __IMPLEMENT_H
#define __IMPLEMENT_H

#include <string>
#include <vector>

typedef std::string                    CC_STRING;
#define GET_STL_VECTOR_CONTAINER()          std::vector<T> &stl_container = * (std::vector<T> *) priv

template<class T>
CC_ARRAY<T>::CC_ARRAY()
{
	priv = new std::vector<T>;
}
	
template<class T>
CC_ARRAY<T>::~CC_ARRAY()
{
	delete (std::vector<T> *) priv;
}

template<class T>
void CC_ARRAY<T>::erase(size_t begin, size_t end)
{
	GET_STL_VECTOR_CONTAINER();
	stl_container.erase(stl_container.begin() + begin, stl_container.begin() + end);
}

template<class T>
void CC_ARRAY<T>::erase(size_t pos)
{
	GET_STL_VECTOR_CONTAINER();
	stl_container.erase(stl_container.begin() + pos);
}

template<class T>
void CC_ARRAY<T>::insert(size_t pos, const T *buf, size_t nr)
{
	GET_STL_VECTOR_CONTAINER();
	stl_container.insert(stl_container.begin() + pos, buf, buf + nr);
}

template<class T>
void CC_ARRAY<T>::insert(size_t pos, const T& x)
{
	GET_STL_VECTOR_CONTAINER();
	stl_container.insert(stl_container.begin() + pos, x);
}

template<class T>
void CC_ARRAY<T>::insert(size_t pos, const CC_ARRAY& array2)
{
	GET_STL_VECTOR_CONTAINER();
    const std::vector<T> &stl_container2 = * (std::vector<T> *) array2.priv;
	stl_container.insert(stl_container.begin() + pos, stl_container2.begin(), stl_container2.end());
}

template<class T>
void CC_ARRAY<T>::push_back(const T& x)
{
	GET_STL_VECTOR_CONTAINER();
	stl_container.push_back(x);
}

template<class T>
bool CC_ARRAY<T>::pop_back(T& x)
{
	GET_STL_VECTOR_CONTAINER();
	if(stl_container.size() == 0)
		return false;
	x = stl_container.back();
	stl_container.pop_back();
	return true;
}

template<class T>
void CC_ARRAY<T>::clear()
{
	GET_STL_VECTOR_CONTAINER();
	stl_container.clear();
}

template<class T>
T& CC_ARRAY<T>::operator [](size_t pos)
{
	GET_STL_VECTOR_CONTAINER();
	return stl_container[pos];
}

template<class T>
const T& CC_ARRAY<T>::operator [](size_t pos) const
{
	GET_STL_VECTOR_CONTAINER();
	return stl_container[pos];
}

template<class T>
size_t CC_ARRAY<T>::size() const
{
	GET_STL_VECTOR_CONTAINER();
	return stl_container.size();
}

template<class T>
T * CC_ARRAY<T>::get_buffer(void)
{
	GET_STL_VECTOR_CONTAINER();
	return & stl_container.front();
}

template<class T>
T& CC_ARRAY<T>::front(void)
{
	GET_STL_VECTOR_CONTAINER();
	return stl_container.front();
}

template<class T>
T& CC_ARRAY<T>::back(void)
{
	GET_STL_VECTOR_CONTAINER();
	return stl_container.back();
}

#undef GET_STL_VECTOR_CONTAINER


#include <map>
#define GET_STL_MAP_CONTAINER()          std::map<T1,T2> &stl_container = * (std::map<T1,T2> *) priv

template<class T1, class T2>
CC_MAP<T1,T2>::CC_MAP()
{
	priv = new std::map<T1,T2>;
}
	
template<class T1, class T2>
CC_MAP<T1,T2>::~CC_MAP()
{
	delete (std::map<T1,T2> *) priv;
}

template<class T1, class T2>
void CC_MAP<T1,T2>::insert(const T1& x1, const T2& x2)
{
	GET_STL_MAP_CONTAINER();
	stl_container.insert(typename std::map<T1,T2>::value_type(x1,x2));
}

template<class T1,class T2>
void CC_MAP<T1,T2>::clear()
{
	GET_STL_MAP_CONTAINER();
	stl_container.clear();
}


template<class T1, class T2>
size_t CC_MAP<T1,T2>::size() const
{
	GET_STL_MAP_CONTAINER();
	return stl_container.size();
}

template<class T1, class T2>
bool CC_MAP<T1,T2>::find(const T1& x1)
{
	GET_STL_MAP_CONTAINER();
	typename std::map<T1,T2>::iterator pos = stl_container.find(x1);
	if(pos == stl_container.end())
		return false;
	return true;
}

template<class T1, class T2>
bool CC_MAP<T1,T2>::find(const T1& x1, T2& x2)
{
	GET_STL_MAP_CONTAINER();
	typename std::map<T1,T2>::iterator pos = stl_container.find(x1);
	if(pos == stl_container.end())
		return false;
	x2 = pos->second;
	return true;
}

#endif


