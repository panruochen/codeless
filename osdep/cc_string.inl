#ifndef __CC_STRING_INL
#define __CC_STRING_INL

inline const CC_STRING operator + (const char *s1, const CC_STRING& s2) 
{ return std::operator+(s1,s2); }

inline const CC_STRING operator + (const char c, const CC_STRING& s) 
{ return std::operator+(c,s); }

inline const CC_STRING operator + (const CC_STRING& s1, const CC_STRING& s2)
{ return std::operator+(s1, s2); }

inline const CC_STRING operator + (const CC_STRING& s1, const char *s2  )
{ return std::operator+(s1, s2); }

inline const CC_STRING operator + (const CC_STRING& s, const char c)
{ return std::operator+(s, c); }

inline bool operator == (const char *s1, const CC_STRING& s2)
{ return std::operator==(s1, s2);  }

inline bool operator == (const CC_STRING& s1, const char *s2) 
{ return std::operator==(s1, s2);  }

inline bool operator == (const CC_STRING& s1, const CC_STRING& s2) 
{ return std::operator==(s1, s2);  }

inline CC_STRING::CC_STRING() : std::string() {}
inline CC_STRING::CC_STRING(const char* s) : std::string(s) {}

inline CC_STRING::CC_STRING(const char *s1, const char *s2) : std::string()
{
	std::string::insert(std::string::begin(), s1, s2);
}

inline CC_STRING::CC_STRING(const std::string& s) : std::string(s)
{
}

inline CC_STRING::CC_STRING(const CC_STRING& s) : std::string(s)
{
}


inline void CC_STRING::strcat(const char *from, const char *to)
{
	insert(end(), from, to);
}

inline void CC_STRING::strcat(CC_STRING s, ssize_t from)
{
	insert(end(), s.begin() + from, s.end());
}

inline const CC_STRING& CC_STRING::operator += (const CC_STRING& s)
{
	std::string::operator+=(s);
	return *this;
}


inline const CC_STRING& CC_STRING::operator += (const char *s)
{
	std::string::operator+=(s);
	return *this;
}

inline const CC_STRING& CC_STRING::operator += (const char c)
{
	std::string::operator+=(c);
	return *this;
}

inline ssize_t CC_STRING::find(const CC_STRING& s)
{
	std::string::size_type pos = std::string::find(s);
	return pos == std::string::npos ? -1 : pos;
}

inline ssize_t CC_STRING::rfind(const char c)
{
	std::string::size_type pos = std::string::rfind(c);
	return pos == std::string::npos ? -1 : pos;
}

inline bool CC_STRING::isnull() const
{
	return std::string::empty();
}

inline CC_STRING& CC_STRING::operator=(const char *s)
{
	if(s == NULL)
		std::string::clear();
	else
		std::string::operator=(s);
	return *this;
}

inline const char * CC_STRING::c_str() const 
{
    if(std::string::size() > 0 )
	   return std::string::c_str();
    return NULL;
}


#endif

