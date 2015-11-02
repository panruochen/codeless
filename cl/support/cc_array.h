#ifndef __CC_ARRAY_H
#define __CC_ARRAY_H

#include <vector>

template <typename T> class CC_ARRAY : private std::vector<T> {
public:
	CC_ARRAY() {}
	void  push_back(const T& x) { std::vector<T>::push_back(x); }

	using std::vector<T>::size;
	using std::vector<T>::clear;
	using std::vector<T>::operator[];
};

template <typename T> class CC_STACK : private std::vector<T> {
public:
	CC_STACK() {}
	T&   top() { return std::vector<T>::back(); }
	void push(const T& x) { std::vector<T>::push_back(x); }
	bool pop(T& x)  { return (std::vector<T>::size() == 0) ? false : (x = std::vector<T>::back(), std::vector<T>::pop_back(), true); }

	using std::vector<T>::clear;
	using std::vector<T>::size;
};

#endif


