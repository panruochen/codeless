#ifndef __CC_STRING_H
#define __CC_STRING_H

#include <sys/types.h>
#include <string>
#include <stdarg.h>

class CC_STRING : private std::string
{
public:
   inline CC_STRING();
   inline CC_STRING(const char* s);
   inline CC_STRING(const char *s1, const char *s2);
   inline CC_STRING(const std::string& s);
   inline CC_STRING(const CC_STRING& s);

   inline void strcat(const char *from, const char *to);
   inline void strcat(CC_STRING s, ssize_t from);

   inline const CC_STRING& operator += (const CC_STRING& s);
   inline const CC_STRING& operator += (const char *s);
   inline const CC_STRING& operator += (const char c);

   friend inline bool operator == (const CC_STRING& s1, const char *s2);
   friend inline bool operator == (const char *s1, const CC_STRING& s2);
   friend inline bool operator == (const CC_STRING& s1, const CC_STRING& s2);

   friend inline const CC_STRING operator + (const CC_STRING& s1, const CC_STRING& s2);
   friend inline const CC_STRING operator + (const char *s1, const CC_STRING& s2);
   friend inline const CC_STRING operator + (const char c, const CC_STRING& s2);
   friend inline const CC_STRING operator + (const CC_STRING& s1, const char *s2);
   friend inline const CC_STRING operator + (const CC_STRING& s, const char c);

   inline ssize_t find(const CC_STRING& s) const;
   inline ssize_t rfind(const char c) const;

   inline CC_STRING& operator=(const char *s);
   inline const char * c_str() const;
   void format(const char *fmt, ...);
   void format(const char *fmt, va_list ap);

   inline bool isnull() const;
   inline void remove_last();

//   CC_STRING& operator=(const CC_STRING& s) { std::string::operator+=(s); return *this; }
//   CC_STRING& operator=(const char c) { std::string::operator+=(c); return *this; }

   using std::string::operator=;
   using std::string::operator[];
   using std::string::size;
   using std::string::clear;
   using std::string::empty;

};

#include "cc_string.inl"


#endif

